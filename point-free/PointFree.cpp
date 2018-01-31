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
		
	CExpr* TransformToCExpr(Decl* d, std::string parent = "") {
	//	errs() << "Entered Decl Transform \n";
	//	d->dump();
	//	errs() << "\n \n \n";
		if (auto* tad = dyn_cast<TypeAliasDecl>(d)) {
			//errs() << " \n";
			//tad->dump();
			//errs() << " \n";
			//errs() << tad->getName() << "\n";
			//errs() << tad->getNameAsString() << "\n";
			//errs() << tad->getIdentifier()->getName() << "\n";
			
			return TransformToCExpr(tad->getUnderlyingType().getTypePtr());
		}	

		// These need to be treated like App's perhaps some of the others do as well.		
		if (auto* crd = dyn_cast<CXXRecordDecl>(d)) { 
			//crd->dump();
			//errs() << crd->getNameAsString() << "\n";
		}
				
		if(auto* ctd = dyn_cast<ClassTemplateDecl>(d)) {	
			//ctd->dump();
			for (DeclContext::decl_iterator i = ctd->getTemplatedDecl()->decls_begin(), e = ctd->getTemplatedDecl()->decls_end(); i != e; i++) {
					CLambda *tCLambdaTop = nullptr, *tCLambdaCurr = nullptr;
					for(TemplateParameterList::iterator i = ctd->getTemplateParameters()->begin(), e = ctd->getTemplateParameters()->end(); i != e; i++) {	
						if (tCLambdaTop == nullptr) {
							tCLambdaTop = tCLambdaCurr = new CLambda(); 
							tCLambdaCurr->pat = new PVar((*i)->getNameAsString()); 
						} else {
							tCLambdaCurr->expr = new CLambda();
							tCLambdaCurr = dynamic_cast<CLambda*>(tCLambdaCurr->expr); 	
							tCLambdaCurr->pat = new PVar((*i)->getNameAsString());
						}
					}
				
			  //(*i)->dump();
					
			   if (auto* nd = dyn_cast<NamedDecl>(*i)) { 

					// This may have to be moved up to the Visit class if 
					// I find that ClassTemplateDecl's occur more than once
					// in a template, in that case it may be best to do a check
					// to exclude CXXRecord's with the same name as the ClassTemplateDecl.
					// As otherwise it will go into its own CXXRecord which i don't think is 
					// correct, at least at the moment. 
					if (nd->getNameAsString() == TypeAliasOrDefName) {
						tCLambdaCurr->expr = TransformToCExpr(*i);
						return tCLambdaTop;
					}
				}
			}
			
			
		}

		return nullptr;
	}
	
	CExpr* TransformToCExpr(const clang::Type* t, std::string parent = "") {
	//	errs() << "Entered Type Transform \n";
		//t->dump();
		//errs() << "\n \n \n";
		
		// could be incorrectly handling this and throwing away 
		// important information, its of the type something<possiblevalue>::type 
		if (auto* dnt = dyn_cast<DependentNameType>(t)) {
			// this gets everything before so Foo<T> etc. not neccesarily ideal.
	//		dnt->getQualifier()->dump();
			
			// This retireves the name after ::, so "::" + getName() would
			// get ::type or so. 
	//		errs() << "\n" << dnt->getIdentifier()->getName() << "\n";
			return TransformToCExpr(dnt->getQualifier()->getAsType(), "dnt");
		} 
		
		// a sugared type, things like std::is_polymorphic<T> have a layer of this
		if (auto* et = dyn_cast<ElaboratedType>(t)) {
			CExpr* temp = TransformToCExpr(et->desugar().getTypePtr());
			if (et->getQualifier()->getAsNamespace()) {
				App* tApp = dynamic_cast<App*>(temp);		
			    Var* tVar = dynamic_cast<Var*>(tApp->exprL);
			    tVar->name =  et->getQualifier()->getAsNamespace()->getNameAsString() + "::" + tVar->name;
			}
			
			return temp;
		}
		
		// same as above, possible loss of information. 
		if (auto* tst = dyn_cast<TemplateSpecializationType>(t)) {
			App* curApp, * topApp; 
			
			// Will they always require an application, what happens if there is no arguements?
			// if (curArg > 0) ?
			curApp = topApp = new App(); 
			
		    int curArg = 0;
			int argCount = tst->getNumArgs() - 1;
			for (TemplateSpecializationType::iterator i = tst->end() - 1, e = tst->begin() - 1; i != e; i--) {
			   if (curArg < argCount) {	
				   curApp->exprL = new App(); 
				   curApp->exprR = new Var(Prefix, (*i).getAsType().getAsString());
				   curApp = dynamic_cast<App*>(curApp->exprL);
			   } else {   				   
					if (parent == "dnt")
						curApp->exprL = TransformToCExpr(tst->getTemplateName().getAsTemplateDecl());
					else
						curApp->exprL = new Var(Prefix, tst->getTemplateName().getAsTemplateDecl()->getName()); 
					
					curApp->exprR = new Var(Prefix, (*i).getAsType().getAsString());					
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
			//ctd->dump();		
	    	errs() << "\n";
	    	CExpr* expr = TransformToCExpr(ctd);
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

        for (Rewriter::buffer_iterator i = rewriter.buffer_begin(), e = rewriter.buffer_end(); i != e; ++i) {
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
