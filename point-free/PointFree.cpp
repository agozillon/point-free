#include "clang/Driver/Options.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Format/Format.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/Type.h"

#include "ClangFormatHelpers/ClangFormatHelpers.h"
#include "Common.h"

#include <vector>
#include <utility>
//#include <functional>
#include <stack>
#include <string>
#include <iostream>

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

// options

static cl::opt<std::string> StructureName(
	"structure",cl::init(""),
	cl::desc("The Structure you wish to be made point free"));

static cl::opt<std::string> TypeAliasOrDefName(
	"typealiasordef",cl::init(""),
	cl::desc("The name of the using or type alias in the class you wish to convert"));
	
Rewriter rewriter;
std::stack<std::pair<std::string, std::string>> QualifierNameStack;
	
class PointFreeVisitor : public RecursiveASTVisitor<PointFreeVisitor> {
private:
    ASTContext *astContext; // used for getting additional AST info
	
	CExpr* TransformToCExpr(NestedNameSpecifier* nns) {		
		if (nns->getKind() == NestedNameSpecifier::SpecifierKind::TypeSpec)
			return TransformToCExpr(nns->getAsType());
		
		if (nns->getKind() == NestedNameSpecifier::SpecifierKind::TypeSpecWithTemplate)
			return TransformToCExpr(nns->getAsType());
		
		if (nns->getKind() == NestedNameSpecifier::SpecifierKind::Identifier
		||  nns->getKind() == NestedNameSpecifier::SpecifierKind::Global
		||	nns->getKind() == NestedNameSpecifier::SpecifierKind::Super
		||	nns->getKind() == NestedNameSpecifier::SpecifierKind::NamespaceAlias
		||	nns->getKind() == NestedNameSpecifier::SpecifierKind::Namespace)
			errs() << "Unhandled NestedNameSpecifier in ForwardNestedNameSpecifier \n"; 
			
		return nullptr;	
	}
			
	CExpr* TransformToCExpr(Expr* e) {
		errs() << "Entered Expr Transform \n";
		e->dump();
		errs() << "\n \n \n";
						
		// this is an interesting case and may not be correct!  
		if (auto* dsdre = dyn_cast<DependentScopeDeclRefExpr>(e)) {
			if (auto* tst = dyn_cast<TemplateSpecializationType>(dsdre->getQualifier()->getAsType())) 
				QualifierNameStack.push(std::make_pair(tst->getTemplateName().getAsTemplateDecl()->getName(), dsdre->getDeclName().getAsString()));	
			else
				errs() << "A non-TemplateSpecializationType passed through \n";
						 
			return TransformToCExpr(dsdre->getQualifier());
		}	
		
		if (auto* ueotte = dyn_cast<UnaryExprOrTypeTraitExpr>(e))
		{		
			if (ueotte->getKind() == UnaryExprOrTypeTrait::UETT_SizeOf)
				return new App(new Var(Prefix, "sizeof"), new Var(Prefix, ueotte->getTypeOfArgument().getAsString()));
				
			if (ueotte->getKind() == UnaryExprOrTypeTrait::UETT_AlignOf)
				return new App(new Var(Prefix, "alignof"), new Var(Prefix, ueotte->getTypeOfArgument().getAsString()));
					
			if (ueotte->getKind() == UnaryExprOrTypeTrait::UETT_OpenMPRequiredSimdAlign 
			 || ueotte->getKind() == UnaryExprOrTypeTrait::UETT_VecStep) 
				errs() << "Unhandled UnaryExprOrTypeTrait \n";
		}
		
		if (auto* dre = dyn_cast<DeclRefExpr>(e))
		{
			if (dre->hasQualifier())
				return TransformToCExpr(dre->getQualifier());
				
			return new Var(Prefix, dre->getDecl()->getNameAsString()); 
		}
		
		if (auto* sope = dyn_cast<SizeOfPackExpr>(e))
		{
			// can getPack() return something like a template?
			return new App(new Var(Prefix, "sizeof..."), new Var(Prefix, "..." + sope->getPack()->getNameAsString()));	
		}
		
		if (auto* cble = dyn_cast<CXXBoolLiteralExpr>(e)) {
			if (cble->getValue())
				return new Var(Prefix, "true"); 
			else 
				return new Var(Prefix, "false");
		}
		
		if (auto* il = dyn_cast<IntegerLiteral>(e)) {			 
			return new Var(Prefix, il->getValue().toString(10, true));					
		}
		
		if (auto* cl = dyn_cast<CharacterLiteral>(e)) {
			std::string s(1, (char)cl->getValue());
			return new Var(Prefix, s);	
		}
		
		return nullptr;	
	}			
	
	CExpr* TransformToCExpr(Decl* d) {
		errs() << "Entered Decl Transform \n";
		d->dump();
		errs() << "\n \n \n";

		if (auto* tad = dyn_cast<TypeAliasDecl>(d)) {	
			return TransformToCExpr(tad->getUnderlyingType().getTypePtr());
		}	

		if (auto* tatd = dyn_cast<TypeAliasTemplateDecl>(d)) {		
			if (isFromTypeTraits(tatd->getNameAsString())) {			
				std::string traitName = tatd->getNameAsString();
				
				if (traitName == std::get<0>(QualifierNameStack.top()) && std::get<1>(QualifierNameStack.top()) == "value") {
					traitName += "::v";				
					
					if (QualifierNameStack.size() > 0)
						QualifierNameStack.pop();
				}
								
				if (traitName == std::get<0>(QualifierNameStack.top()) && std::get<1>(QualifierNameStack.top()) == "type") {
					traitName += "::t";			

					if (QualifierNameStack.size() > 0)
						QualifierNameStack.pop();
				}
							
				return new Var(Prefix, traitName);									
			}
								 
			return TransformToCExpr(tatd->getTemplatedDecl());
		}
		
		
		if (auto* td = dyn_cast<TypedefDecl>(d)) {
			return TransformToCExpr(td->getUnderlyingType().getTypePtr());		
		}
	
		if (auto* vd = dyn_cast<VarDecl>(d)) {
		//	vd->getDeclName().dump();    // gets the :: right hand component
		//	errs() << vd->getIdentifier()->getNameStart() << "\n";
		//	errs() << vd->getName() << "\n";
		//	errs() << vd->hasDefinition() << "\n";
		//	if(vd->getEvaluatedValue())
		//		errs() << "there is an evaluated value \n";
		//	else
		//		errs() << "there is no evaluated value \n";			
			
			if (vd->hasInit())		 
				return TransformToCExpr(vd->getInit());
		}
		
		if (auto* fd = dyn_cast<FieldDecl>(d)) { 
		  if (fd->hasInClassInitializer())
			  return TransformToCExpr(fd->getInClassInitializer()); 			
		}
			
		if (auto* crd = dyn_cast<CXXRecordDecl>(d)) { 
			errs() << "this CXXRecordDecl node could be an problem \n";
			if (std::get<0>(QualifierNameStack.top()) == ""
			 && std::get<1>(QualifierNameStack.top()) == "") {
				return new Var(Prefix, crd->getNameAsString()); 
			}
			
			/*
			for (auto i =crd->decls_begin(), e = crd->decls_end(); i != e; i++) {				
				if (auto* nd = dyn_cast<NamedDecl>(*i)) {
					if (nd->getNameAsString() == TypeAliasOrDefName) {
						TypeAliasOrDefName = "";
						return TransformToCExpr(*i);	
					}
				}	
			}*/	
		}
				
		if (auto* ctd = dyn_cast<ClassTemplateDecl>(d)) {	
			
			if(isFromTypeTraits(ctd->getNameAsString())) {
				std::string traitName = ctd->getNameAsString();
				
				if (traitName == std::get<0>(QualifierNameStack.top()) 
				&& std::get<1>(QualifierNameStack.top()) == "value") {
					traitName += "::v";				
					
					if (QualifierNameStack.size() > 0)
						QualifierNameStack.pop();
				}
								
				if (traitName == std::get<0>(QualifierNameStack.top())
				 && std::get<1>(QualifierNameStack.top()) == "type") {
					traitName += "::t";			

					if (QualifierNameStack.size() > 0)
						QualifierNameStack.pop();
				}
							
				return new Var(Prefix, traitName);									
			}
											
			for (auto i = ctd->getTemplatedDecl()->decls_begin(), e = ctd->getTemplatedDecl()->decls_end(); i != e; i++) {
				CLambda *tCLambdaTop = nullptr, *tCLambdaCurr = nullptr;
				std::string pVarName = "";
				
				for(auto i = ctd->getTemplateParameters()->begin(), e = ctd->getTemplateParameters()->end(); i != e; i++) {	
					pVarName = ((*i)->isParameterPack()) ? ("..." + (*i)->getNameAsString()) : (*i)->getNameAsString();
					
					if (tCLambdaTop == nullptr) {
						tCLambdaTop = tCLambdaCurr = new CLambda(); 
						tCLambdaCurr->pat = new PVar(pVarName); 
					} else {
						tCLambdaCurr->expr = new CLambda();
						tCLambdaCurr = dynamic_cast<CLambda*>(tCLambdaCurr->expr); 	
						tCLambdaCurr->pat = new PVar(pVarName);
					}
				}

			   if (auto* nd = dyn_cast<NamedDecl>(*i)) {
					if (ctd->getNameAsString() == std::get<0>(QualifierNameStack.top())
					 && nd->getNameAsString() == std::get<1>(QualifierNameStack.top())) {
					
						if (QualifierNameStack.size() > 0)
							QualifierNameStack.pop();
										
						tCLambdaCurr->expr = TransformToCExpr(*i);
						return tCLambdaTop;
					}
				}
			}
		}

		return nullptr;
	}
	
	CExpr* TransformToCExpr(const clang::Type* t) {
		errs() << "Entered Type Transform \n";
	    t->dump();
		errs() << "\n \n \n";
						
		if (auto* tt = dyn_cast<TypedefType>(t)) {
			return TransformToCExpr(tt->getDecl());
		}
		
		// could be incorrectly handling this and throwing away 
		// important information, its of the type something<possiblevalue>::type 
		if (auto* dnt = dyn_cast<DependentNameType>(t)) {
			// This retireves the name after ::, so "::" + getName() would
			// get ::type or so. Setting the variable in this case is so that 
			// we can tell which member in the template class is getting invoked
			// so we can search for it specifically and ignore the rest.  
			if (auto* tst = dyn_cast<TemplateSpecializationType>(dnt->getQualifier()->getAsType())) 
				QualifierNameStack.push(std::make_pair(tst->getTemplateName().getAsTemplateDecl()->getName(), dnt->getIdentifier()->getName()));	
			else
				errs() << "A non-TemplateSpecializationType passed through \n";

			return TransformToCExpr(dnt->getQualifier());
		} 
		

		// a sugared type, things like std::is_polymorphic<T> have a layer of this
		if (auto* et = dyn_cast<ElaboratedType>(t)) {
			return TransformToCExpr(et->desugar().getTypePtr());
		}

		if (auto* rt = dyn_cast<RecordType>(t)) {
			return TransformToCExpr(rt->getDecl());	
		}
			
		// Can this be handled exactly the same as templatespecializationtype? 	
		if (auto* dtst = dyn_cast<DependentTemplateSpecializationType>(t)) {
			errs() << "Identifier Info: " << dtst->getIdentifier()->getName() << "\n \n";
			errs() << "Number of Args: " << dtst->getArgs() << "\n \n";
			
			return TransformToCExpr(dtst->getQualifier());
		}
				
		// same as above, possible loss of information. 
		if (auto* tst = dyn_cast<TemplateSpecializationType>(t)) {
			App* curApp, * topApp; 
			
			// Will they always require an application, what happens if there is no arguements?
			// if (curArg > 0) ?
			curApp = topApp = new App(); 

		    int curArg = 0, argCount = tst->getNumArgs() - 1;
			std::string name;	
			for (auto i = tst->end() - 1, e = tst->begin() - 1; i != e; i--) {
				CExpr* expr;
				
				if ((*i).getKind() == TemplateArgument::ArgKind::Type) {
					expr = TransformToCExpr((*i).getAsType().getTypePtr());					
				}
				
				if ((*i).getKind() == TemplateArgument::ArgKind::Expression) {
					expr = TransformToCExpr((*i).getAsExpr());
			    }
			    
	
			    if (curArg < argCount) {		   		
				    curApp->exprL = new App(); 
				    curApp->exprR = expr; 		   
				    curApp = dynamic_cast<App*>(curApp->exprL);
			    } else {   				   
				 	if (tst->getTemplateName().getAsTemplateDecl()->getNameAsString() == std::get<0>(QualifierNameStack.top()) 
						&& std::get<1>(QualifierNameStack.top()) != "") {
						errs() << "1: " << std::get<1>(QualifierNameStack.top()) << "\n";
						curApp->exprL = TransformToCExpr(tst->getTemplateName().getAsTemplateDecl());
					} else {						
						errs() << "2: " << std::get<1>(QualifierNameStack.top()) << "  " << tst->getTemplateName().getAsTemplateDecl()->getName() << "\n";
						curApp->exprL = new Var(Prefix, tst->getTemplateName().getAsTemplateDecl()->getName()); 
					}
					curApp->exprR = expr;							
			    }   
				
			   curArg++;
			}
								
			// perhaps I need to go deeper here rather than return?
			return topApp;	//	return TransformToCExpr(tst->getTemplateName().getAsTemplateDecl());
		}
		
		// a template variable like T 
		if (auto* ttpt = dyn_cast<TemplateTypeParmType>(t)) {
			return new Var(Prefix, ttpt->getIdentifier()->getName());
		}
	

		
		// hard-coded type like Int, float, string
		if (auto* bt = dyn_cast<BuiltinType>(t)) {
			PrintingPolicy pp = PrintingPolicy(LangOptions());
			pp.adjustForCPlusPlus();
			return new Var(Prefix, bt->getNameAsCString(pp));			
		}
		
		return nullptr;
	} 
	
	void Print(Pattern* p) {
		if (p == nullptr)
			std::cout << "nullptr error \n";

		if (PVar* pVar = dynamic_cast<PVar*>(p)) {
			std::cout << " (PVar " << pVar->name << ")";
		}
	}

	void Print(CExpr* expr) {
		if (expr == nullptr)
			std::cout << "nullptr error \n";
		
		if (Var* var = dynamic_cast<Var*>(expr)) {				 
			std::cout << " (Var " << ((var->fix == Fixity::Infix) ? "Infix " : "Prefix ") << var->name << ")";
		}
		
		if (App* app = dynamic_cast<App*>(expr)) {
			std::cout << " (App ";
			Print(app->exprL);
			Print(app->exprR);
			std::cout << ")";
		}
	
		if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
			std::cout << " (Lambda ";
			Print(lambda->pat);
			Print(lambda->expr);
			std::cout << ")";
		}
  }	

public:
    explicit PointFreeVisitor(CompilerInstance *CI) 
      : astContext(&(CI->getASTContext())) // initialize private members
    {
        rewriter.setSourceMgr(CI->getSourceManager(), CI->getLangOpts());	
    }

	// with this you have access to the full tree rooted at the initial node
	// could be easier to use than Visit perhaps. 
//	virtual bool TraverseDecl(Decl *x) {	
//		return true; 
//	}

    // can be used to access the structure as a template declaration
    // however I imagine this will pick up templated functions as well
    // as classes. 
    virtual bool VisitClassTemplateDecl(ClassTemplateDecl* ctd) { 
		
		// Can App's be directly replaced with Eval?	       	
		if (ctd->getNameAsString() == StructureName) { 	
				
			CExpr* expr = TransformToCExpr(ctd);
	
			errs() << "Start of Test \n";
			errs() << "\n";
			errs() << "\n";
	    	errs() << "\n";
	    	 		
   			 		
			// I believe this is the correct output for Grey. 
			//CExpr* e2 = new CLambda(new PVar("T"), new App(new Var(Prefix, "is_polymorphic_t"), new App(new Var(Prefix, "Foo"), new Var(Prefix, "T"))));
		/*				 
			errs() << "\n";
	    	Print(e);
	    	std::cout << "\n";	
	    	e = PointFree(e);
	    	Print(e);
			std::cout << "\n";	
	    	errs() << "\n";
		    errs() << "\n";
			errs() << "\n";
			errs() << "End of Test \n";
		*/	
			
	    	errs() << "ClassTemplateDecl Converted To CExpr: \n";
	    	Print(expr); 
	    	std::cout << "\n";	
	    	errs() << "\n";
	    	errs() << "\n";
	    	errs() << "CExpr After Point Free Conversion: \n";
	    	expr = PointFree(expr);
			Print(expr);
			std::cout << "\n";
			   
        }
             
        return true;
    }
};


class PointFreeASTConsumer : public ASTConsumer {
private:
    PointFreeVisitor *visitor; // doesn't have to be private

public:
    // override the constructor in order to pass CI
    explicit PointFreeASTConsumer(CompilerInstance *CI)
        : visitor(new PointFreeVisitor(CI)) // initialize the visitor
    { }

    // override this to call our ExampleVisitor on the entire source file
    virtual void HandleTranslationUnit(ASTContext &Context) {
        /* we can use ASTContext to get the TranslationUnitDecl, which is
             a single Decl that collectively represents the entire source file */
        visitor->TraverseDecl(Context.getTranslationUnitDecl());
    }

};



class PointFreeFrontendAction : public ASTFrontendAction {
public:
  void EndSourceFileAction() override {
        SourceManager &SM = rewriter.getSourceMgr();
   //     errs() << "** Point Free Conversion for: "
   //                 << SM.getFileEntryForID(SM.getMainFileID())->getName() << " complete" << "\n";

        /*********** Write Out Modified File ***********/
       
        std::string file = SM.getFileEntryForID(SM.getMainFileID())->getName();
        std::size_t pos = file.find_last_of("/\\");
        std::string filePath = file.substr(0,pos);
        std::string fileName = file.substr(pos+1);

        if ((pos = fileName.find_last_of(".cpp"))) {
            fileName = fileName.substr(0, pos - 3) + "_pf.cpp";
        } else { // couldn't isolate the filename 
            fileName = "temp_pf.cpp"; 
        }

        fileName = filePath + "/" + fileName;

        // Now emit the rewritten buffer to a file.
        std::error_code error_code;
        raw_fd_ostream outFile(fileName, error_code, sys::fs::F_None);
        rewriter.getEditBuffer(SM.getMainFileID()).write(outFile); // this will write the result to outFile
        outFile.close();

        /*********** Reformat Written File ***********/

        ErrorOr<std::unique_ptr<MemoryBuffer>> CodeOrErr = MemoryBuffer::getFileOrSTDIN(fileName);                        

        if (std::error_code EC = CodeOrErr.getError()) {
            errs() << EC.message() << "\n";
            return;
        }

        std::unique_ptr<llvm::MemoryBuffer> Code = std::move(CodeOrErr.get());

        std::vector<tooling::Range> Ranges;
        if (fillRanges(Code.get(), Ranges))
            return;

        format::FormattingAttemptStatus Status;
        Replacements FormatChanges = reformat(format::getLLVMStyle(), Code->getBuffer(),
                                              Ranges, fileName, &Status);

        tooling::applyAllReplacements(FormatChanges, rewriter);

        for (auto i = rewriter.buffer_begin(), e = rewriter.buffer_end(); i != e; ++i) {
            auto entry = rewriter.getSourceMgr().getFileEntryForID(i->first); 
            if(entry->getName() == fileName) {
                std::error_code error_code;
                raw_fd_ostream reformatedFile(fileName, error_code, sys::fs::F_None);
                rewriter.getEditBuffer(i->first).write(reformatedFile);
                reformatedFile.close();
         //       errs() << "\n \n \n \n Output Formatted Buffer: \n";
         //       rewriter.getEditBuffer(i->first).write(errs());
            }
        }
   }

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) {
        return std::unique_ptr<PointFreeASTConsumer>(new PointFreeASTConsumer(&CI)); // pass CI pointer to ASTConsumer
    }
};



int main(int argc, const char **argv) {
	
    // parse the command-line args passed to your code
    cl::OptionCategory PointFreeCategory("Point Free Tool Options");
    CommonOptionsParser op(argc, argv, PointFreeCategory);        

    if(!StructureName.size()) {
		errs() << "No structure name stated for conversion, exiting without converting \n"; 
		return -1;
	}
	    
    QualifierNameStack.push(std::make_pair(std::string(""), std::string("")));
    if(!TypeAliasOrDefName.size()) {
		errs() << "Type Alias or TypeDef name not stated, assuming name is: type \n"; 
		QualifierNameStack.push(std::make_pair(std::string(StructureName), std::string("type")));
	} else {
		QualifierNameStack.push(std::make_pair(std::string(StructureName), std::string(TypeAliasOrDefName)));
	}

	errs() << "\n \n STACK SIZE:" << QualifierNameStack.size() << "\n";

    
    // create a new Clang Tool instance (a LibTooling environment)
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());

    // run the Clang Tool, creating a new FrontendAction (explained below)
    int result = Tool.run(newFrontendActionFactory<PointFreeFrontendAction>().get());
      
    return result;
}
