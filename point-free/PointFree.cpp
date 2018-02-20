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

class PointFreeVisitor : public RecursiveASTVisitor<PointFreeVisitor> {
private:
    ASTContext *astContext; // used for getting additional AST info
	
	std::vector<TemplateTypeParmDecl*> ttpdVec;
		
	CExpr* TransformToCExpr(Expr* e) {
		
		// this is an interesting case and may not be correct!  
		if (auto* dsdre = dyn_cast<DependentScopeDeclRefExpr>(e)) {		
			CExpr* temp = TransformToCExpr(dsdre->getQualifier()->getAsType());
			//dsdre->getQualifier()->dump();  // gets the :: left hand component 
			//dsdre->getDeclName().dump();    // gets the :: right hand component
			
			if (dsdre->getDeclName()) {
				App* tApp = dynamic_cast<App*>(temp);		
			    Var* tVar = dynamic_cast<Var*>(tApp->exprL);
			    tVar->name = tVar->name + "_v"; // assuming its a value when it gets here
			} 			
			
			return temp;
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

		if (auto* td = dyn_cast<TypedefDecl>(d)) {
			return TransformToCExpr(td->getUnderlyingType().getTypePtr());		
		}
	
		if (auto* fd = dyn_cast<FieldDecl>(d)) { 
		  if(fd->hasInClassInitializer()) {
			  return TransformToCExpr(fd->getInClassInitializer()); 			
		   }
		}
			
		// This is actually handled in the ClassTemplateDecl section (getTemplatedDecl
		// retrieves it) so if this ever gets triggered then perhaps I should move the 
		// section from ClassTemplateDecl to here
		if (auto* crd = dyn_cast<CXXRecordDecl>(d)) { 
			errs() << "Entered CXXRecordDecl, probably shouldn't occur \n";		
		}
				
		if(auto* ctd = dyn_cast<ClassTemplateDecl>(d)) {	
			// tbis goes through all the base classes and looks for integral_constant, then checks if it has a type_trait
			// as an arguement. This should find metafunctions like is_polymorphic. 
			for (auto i = ctd->getTemplatedDecl()->bases_begin(), e = ctd->getTemplatedDecl()->bases_end(); i != e; i++) {
				if (auto* tst = dyn_cast<TemplateSpecializationType>((*i).getType())) {
					// could also check for the integral_constant aspect of it 
					// in conjunction with the type trait expr to flag it as
					// a std type trait. 					 
					for (auto i2 = tst->begin(), e2 = tst->end(); i2 != e2; ++i2) {
						if ((*i2).getKind() == TemplateArgument::ArgKind::Expression) {
							if (auto* tte = dyn_cast<TypeTraitExpr>((*i2).getAsExpr())) {
								std::string traitName = ctd->getNameAsString() += "_t";
															
								// I don't think value comes down the same path 	
								// however i think they should be handled the same way
								// this may require some thought. Perhaps its possible to move
								// this section into the TemplateSpecializationType area.
								// if (TypeAliasOrDefName == "value")
								//	traitName += "_v";									
								
								TypeAliasOrDefName = "";
							 
								return new Var(Prefix, traitName);
							}								
						}																		
					}		
				}					
			}
												
			for (auto i = ctd->getTemplatedDecl()->decls_begin(), e = ctd->getTemplatedDecl()->decls_end(); i != e; i++) {
				CLambda *tCLambdaTop = nullptr, *tCLambdaCurr = nullptr;
				for(auto i = ctd->getTemplateParameters()->begin(), e = ctd->getTemplateParameters()->end(); i != e; i++) {	
					if (tCLambdaTop == nullptr) {
						tCLambdaTop = tCLambdaCurr = new CLambda(); 
						tCLambdaCurr->pat = new PVar((*i)->getNameAsString()); 
					} else {
						tCLambdaCurr->expr = new CLambda();
						tCLambdaCurr = dynamic_cast<CLambda*>(tCLambdaCurr->expr); 	
						tCLambdaCurr->pat = new PVar((*i)->getNameAsString());
					}
				}

			   if (auto* nd = dyn_cast<NamedDecl>(*i)) {
					if (nd->getNameAsString() == TypeAliasOrDefName) {
						TypeAliasOrDefName = "";
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
				
		// could be incorrectly handling this and throwing away 
		// important information, its of the type something<possiblevalue>::type 
		if (auto* dnt = dyn_cast<DependentNameType>(t)) {
			
			// This retireves the name after ::, so "::" + getName() would
			// get ::type or so. Setting the variable in this case is so that 
			// we can tell which member in the template class is getting invoked
			// so we can search for it specifically and ignore the rest.  
			TypeAliasOrDefName = dnt->getIdentifier()->getName();
			return TransformToCExpr(dnt->getQualifier()->getAsType());
		} 
		
		// a sugared type, things like std::is_polymorphic<T> have a layer of this
		if (auto* et = dyn_cast<ElaboratedType>(t)) {
			return TransformToCExpr(et->desugar().getTypePtr());
	/*
		    // A snippet of code that at least in this case adds the namespace onto the front of the 
		    // variable name. So is_polymorphic -> std::is_polymorphic, if its used however namespaces 
		    // would have to be taken into consideration elsewhere where they may not be easily accessible.
		    // For example when dealing with DependentScopeDeclRefExpr   
			if (et->getQualifier()->getAsNamespace()) {
				App* tApp = dynamic_cast<App*>(temp);		
			    Var* tVar = dynamic_cast<Var*>(tApp->exprL);
			    tVar->name =  et->getQualifier()->getAsNamespace()->getNameAsString() + "::" + tVar->name;
			}
	*/	
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
				if ((*i).getKind() == TemplateArgument::ArgKind::Type)
					name = (*i).getAsType().getAsString();
					
				if ((*i).getKind() == TemplateArgument::ArgKind::Expression)
					name = (*i).getAsExpr()->getType().getAsString();
												
			   if (curArg < argCount) {		   		
				   curApp->exprL = new App(); 
				   curApp->exprR = new Var(Prefix, name);
				   curApp = dynamic_cast<App*>(curApp->exprL);
			   } else {   				   
					if (TypeAliasOrDefName != "")
						curApp->exprL = TransformToCExpr(tst->getTemplateName().getAsTemplateDecl());
					else						
						curApp->exprL = new Var(Prefix, tst->getTemplateName().getAsTemplateDecl()->getName()); 
						
					curApp->exprR = new Var(Prefix, name);							
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
				
				
			/*
			errs() << "Start of Test \n";
			errs() << "\n";
			errs() << "\n";
	    	errs() << "\n";
	    	CExpr* e2 = new CLambda(new PVar("T"), new CLambda(new PVar("T2"), new App(new App(
	    	new CLambda(new PVar("T3"), new CLambda (new PVar("T4"), new Var(Prefix, "T3"))), new Var(Prefix, "T")), new Var(Prefix, "T2"))));
			errs() << "\n";
	    	Print(e2);
	    	std::cout << "\n";	
	    	e2 = PointFree(e2);
	    	Print(e2);
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
    
    if(!TypeAliasOrDefName.size()) {
		errs() << "Type Alias or TypeDef name not stated, assuming name is: type \n"; 
		TypeAliasOrDefName = "type";
	}
	
    if(!StructureName.size()) {
		errs() << "No structure name stated for conversion, exiting without converting \n"; 
		return -1;
	}
    
    // create a new Clang Tool instance (a LibTooling environment)
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());

    // run the Clang Tool, creating a new FrontendAction (explained below)
    int result = Tool.run(newFrontendActionFactory<PointFreeFrontendAction>().get());
      
    return result;
}
