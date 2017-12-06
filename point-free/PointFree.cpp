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

using namespace std;
using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

Rewriter rewriter;
int numFunctions = 0;


class PointFreeVisitor : public RecursiveASTVisitor<PointFreeVisitor> {
private:
    ASTContext *astContext; // used for getting additional AST info

public:
    explicit PointFreeVisitor(CompilerInstance *CI) 
      : astContext(&(CI->getASTContext())) // initialize private members
    {
        rewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());
    }

    virtual bool VisitFunctionDecl(FunctionDecl *func) {
        numFunctions++;
        string funcName = func->getNameInfo().getName().getAsString();
        if (funcName == "do_math") {
            rewriter.ReplaceText(func->getLocation(), funcName.length(), "add5");
            errs() << "** Rewrote function def: " << funcName << "\n";
        }    
        return true;
    }

    virtual bool VisitStmt(Stmt *st) {
        if (ReturnStmt *ret = dyn_cast<ReturnStmt>(st)) {
            rewriter.ReplaceText(ret->getRetValue()->getLocStart(), 6, "val");
            errs() << "** Rewrote ReturnStmt\n";
        }        
        if (CallExpr *call = dyn_cast<CallExpr>(st)) {
            rewriter.ReplaceText(call->getLocStart(), 7, "add5");
            errs() << "** Rewrote function call\n";
        }
        return true;
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
    virtual ASTConsumer *CreateASTConsumer(CompilerInstance &CI, StringRef file) {
        return new PointFreeASTConsumer(&CI); // pass CI pointer to ASTConsumer
    }
};



int main(int argc, const char **argv) {
    // parse the command-line args passed to your code
    CommonOptionsParser op(argc, argv);        
    // create a new Clang Tool instance (a LibTooling environment)
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());

    // run the Clang Tool, creating a new FrontendAction (explained below)
    int result = Tool.run(newFrontendActionFactory<PointFreeFrontendAction>());

    errs() << "\nFound " << numFunctions << " functions.\n\n";
    // print out the rewritten source code ("rewriter" is a global var.)
    rewriter.getEditBuffer(rewriter.getSourceMgr().getMainFileID()).write(errs());
    return result;
}
