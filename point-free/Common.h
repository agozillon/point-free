// Copyright (c) 2018 Andrew Gozillon & Paul Keir, University of the West of Scotland.
#pragma once
#include <string>
#include <vector>
#include <iostream>

////////////////////////////////////////////////////////////////////////
/* Intermediate Point-Free Structure 								  */
////////////////////////////////////////////////////////////////////////

class Pattern {
public:
	virtual ~Pattern() {}
};

class PVar : public Pattern {
public:
	PVar() {}
	PVar(std::string n) { name = n; }
	~PVar() {}

	std::string name;
};

class CExpr {
public:
	virtual ~CExpr() {}
	
	std::string curtainsWrapper;
};

class Var : public CExpr {
public:
	Var(std::string n) { name = n; }
	Var() {}
	virtual ~Var() {}

	std::string name;
};

class CLambda : public CExpr {
public:	
	CLambda(Pattern* p, CExpr* e) { pat = p; expr = e; }
	CLambda() {}
	virtual ~CLambda() { delete pat; delete expr; }
	
	Pattern* pat;
	CExpr* expr;
};

class App : public CExpr {
public:	
	App(CExpr* eL, CExpr* eR) { exprL = eL; exprR = eR; }
	App() {}
	virtual ~App() { delete exprL; delete exprR; }
	
	CExpr* exprL,* exprR;
};

////////////////////////////////////////////////////////////////////////
/* Helpers 								  							  */
////////////////////////////////////////////////////////////////////////

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
		std::cout << " (Var " << var->name << ")";
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

void ConvertNonTypesToMetafunctions(CExpr* expr) {
	if (Var* var = dynamic_cast<Var*>(expr)) {
		if (var->name == "*")
			var->name = "add_pointer_t";
	}

	if (App* app = dynamic_cast<App*>(expr)) {
		ConvertNonTypesToMetafunctions(app->exprL);
		ConvertNonTypesToMetafunctions(app->exprR);
	}

	if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
		ConvertNonTypesToMetafunctions(lambda->expr);
				
		// not sure if I want to swap these to meta-functions 
		// if (PVar* pVar = dynamic_cast<PVar*>(lambda->pat)) {/*pVar->name*/}
	}
}


void Shuffle(CExpr* expr) {
	// if (Var* var = dynamic_cast<Var*>(expr)) {}

	if (App* app = dynamic_cast<App*>(expr)) {
		if (Var* var = dynamic_cast<Var*>(app->exprR)) {
			if(var->name == "add_pointer_t") {
				app->exprR = app->exprL;
				app->exprL = var; 
			}	
		} 
			
		Shuffle(app->exprL);
		Shuffle(app->exprR);
	}

	if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
		Shuffle(lambda->expr);
	}
		
}

// Test with the ones we currently use before commiting to this.
// I might have to add _t and _v variants. 
std::vector<std::string> TypeTraits = {"integral_constant",
									   "bool_constant",
									   "true_type",
									   "false_type",
									   "is_void",
									   "is_null_pointer",
									   "is_integral",
									   "is_floating_point",
									   "is_array",
									   "is_pointer",
									   "is_lvalue_reference",
									   "is_rvalue_reference",
									   "is_member_object_pointer",
									   "is_member_function_pointer",
									   "is_enum",
									   "is_union",
									   "is_class",
									   "is_function",
									   "is_reference",
									   "is_arithmetic",
									   "is_fundamental",
									   "is_object",
									   "is_scalar",
									   "is_compound",
									   "is_member_pointer",
									   "is_const",
									   "is_volatile",
									   "is_trivial",
									   "is_trivially_copyable",
									   "is_standard_layout",
									   "is_pod",
									   "is_empty",
									   "is_polymorphic",
									   "is_abstract",
									   "is_final",
									   "is_aggregate",
									   "is_signed",
									   "is_unsigned",
									   "is_constructible",
									   "is_default_constructible",
									   "is_copy_constructible",
									   "is_move_constructible",
									   "is_assignable",
									   "is_copy_assignable",
									   "is_move_assignable",
									   "is_swappable_with",
									   "is_swappable",
									   "is_destructible",	
									   "is_trivially_constructible",
									   "is_trivially_default_constructible",
									   "is_trivially_copy_constructible",
									   "is_trivially_move_constructible",
									   "is_trivially_assignable",
									   "is_trivially_copy_assignable",
									   "is_trivially_move_assignable",
									   "is_trivially_destructible",							 
									   "is_nothrow_constructible",
									   "is_nothrow_default_constructible",
									   "is_nothrow_copy_constructible",
									   "is_nothrow_move_assignable",
									   "is_nothrow_swappable_with",
									   "is_nothrow_swappable",
									   "is_nothrow_destructible",
									   "has_virtual_destructor",
									   "has_unique_object_representations",
									   "alignment_of",
									   "rank",
									   "extent",
									   "is_same",
									   "is_base_of",
									   "is_convertible",
									   "is_invocable",
									   "is_invocable_r",
									   "is_nothrow_invocable",
									   "is_nothrow_invocable_r",
									   "remove_const",
									   "remove_volatile",
									   "remove_cv",
									   "add_const",
									   "add_volatile",
									   "add_cv",
									   "remove_const_t",
									   "remove_volatile_t",
									   "remove_cv_t",
									   "add_const_t",
									   "add_volatile_t",
									   "add_cv_t",
									   "remove_reference",
									   "add_lvalue_reference",
									   "add_rvalue_reference",
									   "remove_reference_t",
									   "add_lvalue_reference_t",
									   "add_rvalue_reference_t",  
									   "make_signed",
									   "make_unsigned",
									   "make_signed_t",									   
									   "make_signed",
									   "make_unsigned_t",
									   "remove_extent",
									   "remove_all_extents",
									   "remove_extent_t",
									   "remove_all_extents_t",
									   "remove_pointer",
									   "add_pointer",
									   "remove_pointer_t",
									   "add_pointer_t",
									   "aligned_storage",
									   "aligned_union",
									   "decay",
									   "remove_cvref",
									   "enable_if",
									   "conditional",
									   "common_type",
									   "underlying_type",
									   "result_of",
									   "result_of<F(ArgTypes...)>",
									   "invoke_result",
									   "aligned_storage_t",
									   "aligned_union_t",
									   "decay_t",
									   "remove_cvref_t",
									   "enable_if_t", 
									   "conditional_t",
									   "common_type_t",
									   "underlying_type_t",
									   "result_of_t",
									   "invoke_result_t",
									   "void_t",
									   "conjunction",
									   "disjunction",
									   "negation",
									   "is_void_v",
									   "is_null_pointer_v",
									   "is_integral_v",
									   "is_floating_point_v",
									   "is_array_v",
									   "is_pointer_v",
									   "is_lvalue_reference_v",
									   "is_rvalue_reference_v",
									   "is_member_object_pointer_v",
									   "is_member_function_pointer_v",
									   "is_enum_v",
									   "is_union_v",
									   "is_class_v",
									   "is_function_v",
									   "is_reference_v",
									   "is_arithmetic_v",
									   "is_fundamental_v",
									   "is_object_v",
									   "is_scalar_v",
									   "is_compound_v",
									   "is_member_pointer_v",
									   "is_const_v",
									   "is_volatile_v",
									   "is_trivial_v",
									   "is_trivially_copyable_v",
									   "is_standard_layout_v",
									   "is_pod_v",
									   "is_empty_v",					   
									   "is_polymorphic_v",
									   "is_abstract_v",				
									   "is_final_v",
									   "is_aggregate_v",
									   "is_signed_v",
									   "is_unsigned_v",
									   "is_constructible_v",
									   "is_default_constructible_v",
									   "is_copy_constructible_v",
									   "is_move_constructible_v",
									   "is_assignable_v",
									   "is_copy_assignable_v",
									   "is_move_assignable_v",
									   "is_swappable_with_v",
									   "is_swappable_v",
									   "is_destructible_v",
									   "is_trivially_constructible_v",
									   "is_trivially_default_constructible_v",
									   "is_trivially_copy_constructible_v",
									   "is_trivially_move_constructible_v",
									   "is_trivially_assignable_v",
									   "is_trivially_copy_assignable_v",
									   "is_trivially_move_assignable_v",
									   "is_trivially_destructible_v",
									   "is_nothrow_constructible_v",
									   "is_nothrow_default_constructible_v",
									   "is_nothrow_copy_constructible_v",
									   "is_nothrow_move_constructible_v",
									   "is_nothrow_assignable_v",
									   "is_nothrow_copy_assignable_v",
									   "is_nothrow_move_assignable_v",
									   "is_nothrow_swappable_with_v",
									   "is_nothrow_swappable_v",
									   "is_nothrow_destructible_v",
									   "has_virtual_destructor_v",
									   "has_unique_object_representations_v",
									   "alignment_of_v",
									   "rank_v",
									   "extent_v",
									   "is_same_v",
									   "is_base_of_v",
									   "is_convertible_v",
									   "is_invocable_v",
									   "is_invocable_r_v",
									   "is_nothrow_invocable_v",
									   "is_nothrow_invocable_r_v",
									   "conjunction_v",
									   "disjunction_v",
									   "negation_v"	   									   									   
									   };
 
 std::vector<std::string> PrimitiveTypes =   {"short",
											  "short int",
											  "signed short",
											  "signed short int",
											  "unsigned short",
											  "unsigned short int",
											  "int",
											  "signed",
											  "signed int",
											  "unsigned",
											  "unsigned int",
											  "long",
											  "long long", 
											  "long int",
											  "signed long",
											  "signed long int",
											  "unsigned long",
											  "unsigned long int",
											  "long long",
											  "long long int",
											  "signed long long",
											  "signed long long int",
											  "unsigned long long",
											  "unsigned long long int",
											  "float",
											  "double",
											  "long double",
											  "char",
											  "signed char",
											  "unsigned char",
											  "wchar_t",
											  "char16_t",
											  "char32_t",
											  "string",
											  "std::string",
											  "void",
											  "nullptr_t",
											  "std::nullptr_t",
											  "nullptr",
											  "bool",
											  "true",
											  "false"
											 };
											 
 std::vector<std::string> CombinatorOrPreludeNames = { "const_",
													   "id",
													   "S",
													   "fix",
													   "compose",
													   "cons",
													   "dollar",
													   "flip",
													   "foldl",
													   "foldr",
													   "get",
													   "if_",
													   "map",
													   "fmap_tree"
													 };
											 

// Create a Function is type traits that accepts a string, loops through
// the array above and returns true. 
bool isFromTypeTraits(std::string name) {
	std::size_t found = name.find_last_of("::");
	std::string modName = name.substr(0, found - 1);
	for (unsigned int i = 0; i < TypeTraits.size(); ++i) {
		if (name == TypeTraits[i] || modName == TypeTraits[i])
			return true;
	}
	return false;
}

bool isAPrimitiveType(std::string name) {										
	for (unsigned int i = 0; i < PrimitiveTypes.size(); ++i) {
		if (name == PrimitiveTypes[i])
			return true;
	}
	return false;
}

bool isACombinatorOrPrelude(std::string name) {										
	for (unsigned int i = 0; i < CombinatorOrPreludeNames.size(); ++i) {
		if (name == CombinatorOrPreludeNames[i])
			return true;
	}
	return false;
}



////////////////////////////////////////////////////////////////////////
/* Point-Free Algorithm 											  */
////////////////////////////////////////////////////////////////////////

CExpr* TransformRecursive(CExpr* expr, std::vector<std::string> names);
CExpr* RemoveVariable(std::string name, std::vector<std::string> names, CExpr* expr);

std::string MapSize(std::map<std::string, std::stack<std::string>> env) {
	int size = 0;
	
	for (auto it = env.begin(); it != env.end(); ++it)
		size += it->second.size();
	
	return std::to_string(size);
}

std::string charGen(int index) {
	std::string result;
	while (index >= 0) {
		result = (char)('a' + index % 26) + result;
		index /= 26;
		index--;
	}
	return result;
}

std::string fresh(std::vector<std::string> strList) {
	unsigned int i = 0, genSeed = 0;
	std::string cGen = charGen(genSeed);

	while (i < strList.size())
	{
		if (strList[i] != cGen) {
			++i;
		} else {
			++genSeed;
			cGen = charGen(genSeed);
			i = 0;
		}
	}
		
	return cGen;
}

std::vector<std::string> AlphaPat(Pattern* pat, std::map<std::string, std::stack<std::string>>& env) {
	
	std::vector<std::string> nameList, tempList;

	/*alphaPat(PVar v) = do
	  fm <-get 
	  let v' = "$" ++ show (M.size fm) 
	  put $ M.insert v v' fm 
	  return $ PVar v' */
	if (PVar* pVar = dynamic_cast<PVar*>(pat)) {
		std::string newName = "$" + MapSize(env);

		// This has to be changed, we won't always be inserting a new element now.
		// And the element is a stack.
		auto it = env.find(pVar->name);
		if (it != env.end()) {
			it->second.push(newName);
		} else {
			std::stack<std::string> newStack; 
			newStack.push(newName);
			env.insert(std::pair<std::string, std::stack<std::string>>(pVar->name, newStack));
		}
		
		nameList.push_back(pVar->name);
		pVar->name = newName;
	}
	
	return nameList;
}

void Alpha(CExpr* expr, std::map<std::string, std::stack<std::string>>& env) {
	// alpha(Var f v) = do fm <-get; return $ Var f $ maybe v id(M.lookup v fm)
	if (Var* var = dynamic_cast<Var*>(expr)) {
		auto it = env.find(var->name);
		if (it != env.end())
			var->name = it->second.top();
	}

	// alpha(App e1 e2) = liftM2 App(alpha e1) (alpha e2)
	if (App* app = dynamic_cast<App*>(expr)) {
		Alpha(app->exprL, env);
		Alpha(app->exprR, env);
	}

	// alpha(Lambda v e') = inEnv $ liftM2 Lambda (alphaPat v) (alpha e')
	if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
		std::vector<std::string> popList = AlphaPat(lambda->pat, env);
		Alpha(lambda->expr, env);

		for (size_t i = 0; i < popList.size(); ++i) {
			auto it = env.find(popList[i]);
			if (it != env.end()) {
				it->second.pop();
			}
		}

	}
}

void AlphaRename(CExpr* expr) {
	// Key = Old name, Value = new $1 identifier 
	std::map<std::string, std::stack<std::string>> env;
	Alpha(expr, env);
}

void gatherNames(CExpr* expr, std::vector<std::string>& names) {
	if (Var* var = dynamic_cast<Var*>(expr)) {
		names.push_back(var->name);
	}
	
	if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
		gatherNames(lambda->expr, names);
	}

	if (App* app = dynamic_cast<App*>(expr)) {
		gatherNames(app->exprL, names);
		gatherNames(app->exprR, names);
	}	
}

bool occursInPattern(std::string name, Pattern* p) {
	bool ret = false;

	if (PVar* pVar = dynamic_cast<PVar*>(p)) {
		ret = (pVar->name == name);
	}

	return ret;
}

int freeIn(std::string name, CExpr* expr) {
	int ret = 0;

	if (Var* var = dynamic_cast<Var*>(expr)) {
		ret = (name == var->name);
	}
	
	if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
		if (occursInPattern(name, lambda->pat))
			ret = 0;
		else
			ret = freeIn(name, lambda->expr);
	}

	if (App* app = dynamic_cast<App*>(expr)) {
		ret = freeIn(name, app->exprL) + freeIn(name, app->exprR);
	}

	return ret;
}

bool isFreeIn(std::string name, CExpr* expr) {
	return (freeIn(name, expr) > 0);
}

CExpr* RemoveVariable(std::string name, std::vector<std::string> names, CExpr* expr) {
	if (Var* var = dynamic_cast<Var*>(expr)) {
		if (name == var->name) {
			delete var;
			return new Var("id");
		} else {					
			return new App(new Var("const_"), var);
		}
	}

	if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
		if (!occursInPattern(name, lambda->pat)) {
			return RemoveVariable(name, names, TransformRecursive(expr, names));
		} else {
			assert(false);
		}
		return expr; // should never actually occur
	}

	if (App* app = dynamic_cast<App*>(expr)) {
		bool frL = isFreeIn(name, app->exprL);
		bool frR = isFreeIn(name, app->exprR);
		Var* vR = dynamic_cast<Var*>(app->exprR);
		CExpr* temp;
		
		if (frL && frR) {
			app->exprL = RemoveVariable(name, names, app->exprL);
			app->exprR = RemoveVariable(name, names, app->exprR);
			temp = new App(new App(new Var("S"), app->exprL), app->exprR); // S combinator, instead of Haskell's ap monad 
			app->exprL = nullptr; app->exprR = nullptr; delete app;
			return temp;
		} else if (frL) {
			app->exprL = RemoveVariable(name, names, app->exprL);
			temp = new App(new App(new Var("flip"), app->exprL), app->exprR);
			app->exprL = nullptr; app->exprR = nullptr; delete app;
			return temp;
		} else if (vR && vR->name == name) {
			// can possibly delete exprR
			return app->exprL;
		} else if (frR) {
			app->exprR = RemoveVariable(name, names, app->exprR);
			temp = new App(new App(new Var("compose"), app->exprL), app->exprR); // the compose metafunction instead of Haskell .
			app->exprL = nullptr; app->exprR = nullptr; delete app;
			return temp;
		} else {
			return new App(new Var("const_"), app); // the const_ metafunction instead of haskell const (const is also a reserved word in C++)
		}
	}
	
	return nullptr;
}

CExpr* TransformRecursive(CExpr* expr, std::vector<std::string> names) {
	if (Var* var = dynamic_cast<Var*>(expr)) {
		return var;
	}

	if (App* app = dynamic_cast<App*>(expr)) {
		app->exprL = TransformRecursive(app->exprL, names);
		app->exprR = TransformRecursive(app->exprR, names);
		return expr;
	}

	if (CLambda* lambda = dynamic_cast<CLambda*>(expr)) {
		if (PVar* pVar = dynamic_cast<PVar*>(lambda->pat)) {
			return TransformRecursive(RemoveVariable(pVar->name, names, lambda->expr), names);
		}
	}
	
	return nullptr;
}

CExpr* Transform(CExpr* expr) {
	std::vector<std::string> nameList;
	gatherNames(expr, nameList);
	ConvertNonTypesToMetafunctions(expr);
	Shuffle(expr);
	return TransformRecursive(expr, nameList);
}

CExpr* PointFree(CExpr* expr) {
	AlphaRename(expr);
	return Transform(expr);
}






