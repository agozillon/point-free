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

		if (auto* ctpsd = dyn_cast<ClassTemplatePartialSpecializationDecl>(d)) {			
			if (isFromTypeTraits(ctpsd->getNameAsString())) {
				auto traitName = ctpsd->getNameAsString();
				
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
			
			for (auto i = ctpsd->decls_begin(), e = ctpsd->decls_end(); i != e; i++) {					
				CLambda *tCLambdaTop = nullptr, *tCLambdaCurr = nullptr;
				std::string pVarName = "";
				
				for(auto i = ctpsd->getTemplateParameters()->begin(), e = ctpsd->getTemplateParameters()->end(); i != e; i++) {	
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
					if (ctpsd->getNameAsString() == std::get<0>(QualifierNameStack.top())
					 && nd->getNameAsString() == std::get<1>(QualifierNameStack.top())) {
					
						if (QualifierNameStack.size() > 0)
							QualifierNameStack.pop();
										
						tCLambdaCurr->expr = TransformToCExpr(*i);
						return tCLambdaTop;
					}
				}
			}
				
			return nullptr;
		}
		
		// I don't treat full specialization as a lambda as technically it has no template parameters 
		// (they're all fixed/constant arguments) I'm unsure of a case where the specialized arguments 
		// would have to be treated as lambda parameters 
		if (auto* ctsd = dyn_cast<ClassTemplateSpecializationDecl>(d)) {
			if(isFromTypeTraits(ctsd->getNameAsString())) {
				std::string traitName = ctsd->getNameAsString();
				
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
			
			for (auto i = ctsd->decls_begin(), e = ctsd->decls_end(); i != e; i++) {					
			    if (auto* nd = dyn_cast<NamedDecl>(*i)) {
					if (ctsd->getNameAsString() == std::get<0>(QualifierNameStack.top())
					&& nd->getNameAsString() == std::get<1>(QualifierNameStack.top())) {
				
						if (QualifierNameStack.size() > 0)
							QualifierNameStack.pop();
										
						return TransformToCExpr(*i);
					}
				}
			}
		
			return nullptr;			
		}

		if (auto* tad = dyn_cast<TypeAliasDecl>(d)) {	
			
			errs() << "\n \n !!!!!Printing Qualified String!!!!! \n \n" << tad->getQualifiedNameAsString() << "\n \n !!!!!Printing Qualified String!!!!! \n \n";
			
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
			if (vd->hasInit())		 
				return TransformToCExpr(vd->getInit());
		}
		
		if (auto* fd = dyn_cast<FieldDecl>(d)) {
		 
		  if (fd->hasInClassInitializer())
			  return TransformToCExpr(fd->getInClassInitializer()); 			
		}
			
		if (auto* crd = dyn_cast<CXXRecordDecl>(d)) { 
			errs() << "this CXXRecordDecl node could be a problem \n";
			if (std::get<0>(QualifierNameStack.top()) == ""
			 && std::get<1>(QualifierNameStack.top()) == "") {
				return new Var(Prefix, crd->getNameAsString()); 
			}	
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

		if (auto* pt = dyn_cast<clang::PointerType>(t)) {
			CExpr* expr = TransformToCExpr(pt->getPointeeType().getTypePtr());
			
			App* app = new App();
			app->exprL = expr;
			app->exprR = new Var(Prefix, "*"); 
			expr = app;
		
			return expr;
		}		
		
		if (auto* pet = dyn_cast<PackExpansionType>(t)) {			
			CExpr* expr = TransformToCExpr(pet->getPattern().getTypePtr());
			
			if (auto* var = dynamic_cast<Var*>(expr)) 	
				var->name = "..." + var->name;
							  
			return expr; 
		}

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
			
		if (auto* dtst = dyn_cast<DependentTemplateSpecializationType>(t)) {		
			return TransformToCExpr(dtst->getQualifier());			
		}
					
		if (auto* sttpt = dyn_cast<SubstTemplateTypeParmType>(t)) {
			return TransformToCExpr(sttpt->getReplacementType().getTypePtr());
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
				CExpr* expr = nullptr;
				
				if ((*i).getKind() == TemplateArgument::ArgKind::Type) {
					expr = TransformToCExpr((*i).getAsType().getTypePtr());					
				}
				
				if ((*i).getKind() == TemplateArgument::ArgKind::Expression) {
					expr = TransformToCExpr((*i).getAsExpr());
			    }
				
				if ((*i).getKind() == TemplateArgument::ArgKind::Template) {
												
					auto* ctd = dyn_cast<ClassTemplateDecl>((*i).getAsTemplate().getAsTemplateDecl());
					
					// The issue with the below is that it will check the template in depth and consider it a lambda
					// which isn't always what we wish to do.
					// 1) GOES INTO FOLDR_C is this what we want? Could be...
					// 2) I might also have to remove the quote_c calls when i find them, as I'm not sure if it changes the meaning of the program?
					// 3) Is there a way to make it automatically look for ::type when finding a quote_c instead of having this if statement
					if (ctd != nullptr && 
						tst->getTemplateName().getAsTemplateDecl()->getName() == "quote_c" && 
						ctd->isThisDeclarationADefinition()) {
						QualifierNameStack.push(std::make_pair((*i).getAsTemplate().getAsTemplateDecl()->getName(), "type"));			
						expr = TransformToCExpr((*i).getAsTemplate().getAsTemplateDecl()); 
					} else {
						expr = new Var(Prefix, (*i).getAsTemplate().getAsTemplateDecl()->getName()); 
					}
			    }

				if ((*i).getKind() == TemplateArgument::ArgKind::TemplateExpansion) {
					errs() << "ArgKind::TemplateExpansion unhandled \n";
			    }
			    	
			    if (curArg < argCount) {			
				    curApp->exprL = new App(); 
				    curApp->exprR = expr; 		   
				    curApp = dynamic_cast<App*>(curApp->exprL);
			    } else {   				   
				 	if (tst->getTemplateName().getAsTemplateDecl()->getNameAsString() == std::get<0>(QualifierNameStack.top()) 
						&& std::get<1>(QualifierNameStack.top()) != "") {
						curApp->exprL = TransformToCExpr(tst->getTemplateName().getAsTemplateDecl());
					} else {						
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
	
	CExpr* RemoveCurtainsFromCExpr(CExpr* expr) {
		if (expr == nullptr)
			return nullptr;
			
		if (Var* var = dynamic_cast<Var*>(expr)) {}
		
		if (App* app = dynamic_cast<App*>(expr)) {
			if (Var* var = dynamic_cast<Var*>(app->exprL)) {					
				// remove quote/quote_c/eval				
				if (var->name == "quote" || var->name == "quote_c" || var->name == "eval") {
					CExpr* temp = RemoveCurtainsFromCExpr(app->exprR);
					// delete parent node and left node, retain right node. 
					app->exprR = nullptr; 
					delete app;
					return temp; 
				}
			} else {
				app->exprL = RemoveCurtainsFromCExpr(app->exprL); 
				app->exprR = RemoveCurtainsFromCExpr(app->exprR);		
			}
		}
	
		if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
			// RemoveCurtainsFromCExpr(lambda->pat); // We shouldn't have to deal with patterns
			lambda->expr = RemoveCurtainsFromCExpr(lambda->expr);
		}
		
		return expr;
	}
	
	
	std::string ConvertCExprToCurtains(Pattern* p) {
		std::string ret = "";
		
		if (p == nullptr)
			ret += "nullptr error";

		if (PVar* pVar = dynamic_cast<PVar*>(p)) {
			ret += " (PVar " + pVar->name + ")";
		}
		
		return ret;
	}
	
  	std::string ConvertCExprToCurtains(CExpr* expr) {
		std::string ret = "";
		
		if (expr == nullptr)
			ret += "nullptr error";
		
		// (var->fix == Fixity::Infix), need to use this for logic
		// need a function for this
		if (Var* var = dynamic_cast<Var*>(expr)) {				 	
			if (isFromTypeTraits(var->name)) {
				std::size_t found = var->name.find_last_of("::");
				
				if (found != std::string::npos) { 
					if (var->name.substr(found+1) == "t") // ::t
						ret += "quote_c<std::" + var->name.substr(0, found - 1) +">";	
					else if (var->name.substr(found+1) == "v") // ::v 
						ret += "unhandled value \n";
					  
				} else {
					ret += "quote<std::" + var->name + ">";			
				}
				
			} else { 		
				if (isAPrimitiveType(var->name) 
				 || isACombinatorOrPrelude(var->name)) { // is an int, float etc.
					ret += var->name;	
				} else {
					// I don't look for ::v, ::t in this case as these should be user specified metafunctions that 
					// we can simplify and make point-free (so no need to use quote_c or state there is an error).
					ret += "quote<" + var->name + ">"; 
				}			
			}
		}
		
		if (App* app = dynamic_cast<App*>(expr)) {
			ret += "eval<" + ConvertCExprToCurtains(app->exprL) + "," + ConvertCExprToCurtains(app->exprR) + ">";
		}
	
		if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
			assert(false);
		}
		
		return ret;
	}
	
	std::string ConvertToCurtains(CExpr* expr) {
		return ConvertCExprToCurtains(expr);
	}
	

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
		
		  	 
		// Can App's be directly replaced with Eval?	       	
		if (ctd->getNameAsString() == StructureName) { 		    	
			auto topCopy = std::make_pair(std::get<0>(QualifierNameStack.top()), std::get<1>(QualifierNameStack.top()));
			std::cout << "\n \n \n";
	    	std::cout << "ClassTemplateDecl Converted To CExpr: \n";
			CExpr* expr = TransformToCExpr(ctd);	    	
	    	Print(expr); 
			std::cout << "\n \n \n Removed Curtains Calls From CExpr: \n";
	    	expr = RemoveCurtainsFromCExpr(expr);
			Print(expr); 
	    	std::cout << "\n \n \n CExpr After Point Free Conversion: \n";
	    	expr = PointFree(expr);
			Print(expr);
	    	std::cout << "\n \n Print Curtains Lambda: \n" << ConvertToCurtains(expr) << "\n";
			QualifierNameStack.push(topCopy);
	    }
    	
    			         
        return true;
    }
   		
    virtual bool VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* ctsd) {
		if (ctsd->getNameAsString() == StructureName) { 
			auto topCopy = std::make_pair(std::get<0>(QualifierNameStack.top()), std::get<1>(QualifierNameStack.top()));
			std::cout << "\n \n \n";
			CExpr* expr = TransformToCExpr(ctsd);
	    	std::cout << "ClassTemplateSpecializationDecl Converted To CExpr: \n";
			Print(expr);
			std::cout << "\n \n \n Removed Curtains Calls From CExpr: \n";
			expr = RemoveCurtainsFromCExpr(expr);
			Print(expr);
			std::cout << "\n \n \n CExpr After Point Free Conversion: \n";
	    	expr = PointFree(expr);
			Print(expr);
	    	std::cout << "\n \n Print Curtains Lambda: \n" << ConvertToCurtains(expr) << "\n \n";
	    	
			QualifierNameStack.push(topCopy);
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
