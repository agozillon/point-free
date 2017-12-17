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

#include "ClangFormatHelpers/ClangFormatHelpers.h"

#include <vector>
#include <string>

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
        rewriter.setSourceMgr(CI->getSourceManager(), CI->getLangOpts());
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
                // getLocEnd() for TemplateTypeParmDecls modifies the parameters name no matter its length interestingly.
                // this also applies to TypeAliasDecl's (Using statements)
                // rewriter.ReplaceText(ttpd->getLocEnd(), "L");
            }
        }

        for (DeclContext::decl_iterator i = ctd->getTemplatedDecl()->decls_begin(), e = ctd->getTemplatedDecl()->decls_end(); i != e; i++) {
            // TypedefDecl (typedef) && TypeAliasDecl (using) inherit from TypedefNameDecl,
            // so if the code is equivelant for both it is possible to merge them into one.
            if (auto* tad = dyn_cast<TypeAliasDecl>(*i)) { 
                if (auto* ttpt = dyn_cast<TemplateTypeParmType>(tad->getUnderlyingType().getTypePtr())) {
                   for (int i = 0; i < vecTTPD.size(); ++i) {
                       // Can perhaps use getIdentifier to get the length of the type/variables name
                       if (ttpt->getIdentifier()->getName() == vecTTPD.data()[i]->getIdentifier()->getName()) {
                           rewriter.ReplaceText(tad->getLocEnd(), "Y");
                 
                           if (i < vecTTPD.size()) {
                            // EMAIL CLANG MAILING LIST ASK IF THERE IS AN APPROPRIATE WAY TO DELETE WHITE SPACE
                            // THIS WORK AROUND IS.... NOT IDEAL TO SAY THE LEAST!
                            SourceRange temp = vecTTPD.data()[i]->getSourceRange();
                            temp.setEnd(temp.getEnd().getLocWithOffset(2));
                           // rewriter.InsertTextAfter(temp.getEnd(), "D"); // NEEDS TO BE MORE AUTOMATED                    
                            rewriter.RemoveText(temp);
                           // rewriter.ReplaceText(vecTTPD.data()[i + 1]->getSourceRange().getBegin().getLocWithOffset(-1), "");                           
                           }

                            //rewriter.ReplaceText(SourceRange(SourceLocation::getFromRawEncoding(vecTTPD.data()[i]->getLocEnd().getLocWithOffset(1).getRawEncoding()), SourceLocation::getFromRawEncoding(vecTTPD.data()[i + 1]->getLocStart().getLocWithOffset(-1).getRawEncoding())), ""); 
                             
                       }
                   }
                }
                 
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
  void EndSourceFileAction() override {
        SourceManager &SM = rewriter.getSourceMgr();
        errs() << "** Point Free Conversion for: "
                    << SM.getFileEntryForID(SM.getMainFileID())->getName() << " complete" << "\n";

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
                errs() << "\n \n \n \n Output Formatted Buffer: \n";
                rewriter.getEditBuffer(i->first).write(errs());
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
    
    // create a new Clang Tool instance (a LibTooling environment)
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());

    // run the Clang Tool, creating a new FrontendAction (explained below)
    int result = Tool.run(newFrontendActionFactory<PointFreeFrontendAction>().get());
      
    return result;
}
