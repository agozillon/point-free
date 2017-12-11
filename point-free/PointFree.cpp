#include "clang/Driver/Options.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <vector>

using namespace std;
using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

Rewriter rewriter;

class PointFreeVisitor : public RecursiveASTVisitor<PointFreeVisitor> {
private:
    ASTContext *astContext; // used for getting additional AST info

public:
    explicit PointFreeVisitor(CompilerInstance *CI) 
      : astContext(&(CI->getASTContext())) // initialize private members
    {
        rewriter.setSourceMgr(CI->getSourceManager(), CI->getLangOpts()/*astContext->getSourceManager(), astContext->getLangOpts()*/);
    }

    // can be used to access the structure as a template declaration
    // however I imagine this will pick up templated functions as well
    // as classes. 
    virtual bool VisitClassTemplateDecl(ClassTemplateDecl* ctd) { 
        ctd->dump();

        std::vector<TemplateTypeParmDecl*> vecTTPD;

        for(TemplateParameterList::iterator i = ctd->getTemplateParameters()->begin(), e = ctd->getTemplateParameters()->end(); i != e; i++) {
            // find Template Parameters 
            if (auto* ttpd = dyn_cast<TemplateTypeParmDecl>(*i)) { 
                vecTTPD.push_back(ttpd);
                //rewriter.ReplaceText(declT->getLocStart(), "L");
            }
        }

        for (DeclContext::decl_iterator i = ctd->getTemplatedDecl()->decls_begin(), e = ctd->getTemplatedDecl()->decls_end(); i != e; i++) {
            // TypedefDecl (typedef) && TypeAliasDecl (using) inherit from TypedefNameDecl,
            // so if the code is equivelant for both it is possible to merge them into one.
            if (auto* tad = dyn_cast<TypeAliasDecl>(*i)) {
                //tad->getCanonicalDecl()->dump();
                // changes the "correct" thing, however can't really arbitrarily modify it
                // without knowing more about what its type consists of / is.
                // rewriter.ReplaceText(tad->getLocEnd(), "WOOP");
            }

            if (auto* td = dyn_cast<TypedefDecl>(*i)) {
            }
        }
        
        return true;
    }

    // can check what the index is of a TypeParm in a template list, for example  
    // template <typename X, typename Y> - X is index 0, Y is index 1
    // can also detect if its referenced (in use) in the template structure
    // it is in 
    // Picks up template type parameters, inside template <> declarations
    virtual bool VisitTemplateTypeParmDecl(TemplateTypeParmDecl* ttpd) {
//        errs() << "TemplateTypeParmDecl: " << ttpd->getNameAsString() << "\n";

        return true;
    }

    // The Using statement
    virtual bool VisitTypeAliasDecl(TypeAliasDecl* tad) {
//        errs() << "TypeAliasDecl: " << tad->getNameAsString() << "\n";

        return true;
    }

    // can be used to pick up the template structures defintion, but in this case
    // its not as a template, but a basic record like a structure or class. 
    virtual bool VisitCXXRecordDecl(CXXRecordDecl* crd) {
//        errs() << "CXXRecordDecl: " << crd->getNameAsString() << "\n";

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
    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) {
        return std::unique_ptr<PointFreeASTConsumer>(new PointFreeASTConsumer(&CI)); // pass CI pointer to ASTConsumer
    }
};



int main(int argc, const char **argv) {
    // parse the command-line args passed to your code
    cl::OptionCategory PointFreeCategory("Point Free Tool Options");
    CommonOptionsParser op(argc, argv, PointFreeCategory);        
    
    // create a new Clang Tool instance (a LibTooling environment)
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());

    // run the Clang Tool, creating a new FrontendAction (explained below)
    int result = Tool.run(newFrontendActionFactory<PointFreeFrontendAction>().get());
  
    // print out the rewritten source code ("rewriter" is a global var.)
    // This call segmentation faults if no rewritting has been done..I wonder why?
    rewriter.getEditBuffer(rewriter.getSourceMgr().getMainFileID()).write(errs());
    
    return result;
}
