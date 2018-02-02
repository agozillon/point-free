#pragma once
#include <string>
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

class PCons : public Pattern {
public:
	PCons() {}
	PCons(Pattern* p1, Pattern* p2) { pat1 = p1; pat2 = p2; }
	~PCons() { delete pat1;  delete pat2; }

	Pattern* pat1,* pat2;
};

class PTuple : public Pattern {
public:
	PTuple() {}
	PTuple(Pattern* p1, Pattern* p2) { pat1 = p1; pat2 = p2; }
	~PTuple() { delete pat1; delete pat2; }

	Pattern* pat1,* pat2;
};

enum Fixity {
	Prefix,
	Infix
};

class CExpr {
public:
	virtual ~CExpr() {}
};

class CDecl {
public:
	CDecl() {}
	CDecl(std::string n, CExpr* dE) { declName = n; declExpr = dE; }
	~CDecl() { delete declExpr; }

	std::string declName;
	CExpr* declExpr;
};

class Var : public CExpr {
public:
	Var(Fixity f, std::string n) { fix = f; name = n; }
	Var() {}
	virtual ~Var() {}

	Fixity fix;
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

class Let : public CExpr {
public:
	Let(std::vector<CDecl> d, CExpr* e) { decl = d; expr = e; }
	Let() {}
	virtual ~Let() { delete expr; }

	std::vector<CDecl> decl;
	CExpr* expr;
};

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
	
	// alphaPat(PTuple p1 p2) = liftM2 PTuple(alphaPat p1) (alphaPat p2)
	if (PTuple* pTup = dynamic_cast<PTuple*>(pat)) {
		tempList = AlphaPat(pTup->pat1, env);
		
		for (size_t i = 0; i < tempList.size(); i++) {
			nameList.push_back(tempList[i]);
		}

		tempList = AlphaPat(pTup->pat2, env);
		
		for (size_t i = 0; i < tempList.size(); i++) {
			nameList.push_back(tempList[i]);
		}
	}

	// alphaPat(PCons p1 p2) = liftM2 PCons(alphaPat p1) (alphaPat p2)
	if (PCons* pCons = dynamic_cast<PCons*>(pat)) {
		tempList = AlphaPat(pCons->pat1, env);
		
		for (size_t i = 0; i < tempList.size(); i++) {
			nameList.push_back(tempList[i]);
		}
		
		tempList = AlphaPat(pCons->pat2, env);

		for (size_t i = 0; i < tempList.size(); i++) {
			nameList.push_back(tempList[i]);
		}
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

	// alpha(Let _ _) = assert False bt, everything should be broken into a lambda at this point 
	//if (Let* let = dynamic_cast<Let*>(expr)) {
	//	assert(false);
	//}
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
	
	if (Let* let = dynamic_cast<Let*>(expr)) {
		for (auto& decl : let->decl) {
			names.push_back(decl.declName);
			gatherNames(decl.declExpr, names);
		}

		gatherNames(let->expr, names);
	}
}

bool occursInPattern(std::string name, Pattern* p) {
	bool ret = false;
	if (PTuple* pTup = dynamic_cast<PTuple*>(p)) {
		ret = (occursInPattern(name, pTup->pat1) ||
				occursInPattern(name, pTup->pat2));
	}

	if (PCons* pCons = dynamic_cast<PCons*>(p)) {
		ret = (occursInPattern(name, pCons->pat1) || 
				occursInPattern(name, pCons->pat2));
	}

	if (PVar* pVar = dynamic_cast<PVar*>(p)) {
		ret = (pVar->name == name);
	}

	return ret;
}

bool occursInDeclList(std::string name, std::vector<CDecl> declList) {
	for (auto val : declList) {
		if (val.declName == name)
			return true;
	}
	return false;
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

	if (Let* let = dynamic_cast<Let*>(expr)) {
		if (occursInDeclList(name, let->decl)) {
			ret = 0;
		} else {
			ret = freeIn(name, let->expr);
			for (auto var : let->decl) {
				ret += freeIn(name, var.declExpr);
			}
		}
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
			return new Var(Prefix, "id");
		} else {					
			return new App(new Var(Prefix, "const"), var);
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

	//if (Let* let = dynamic_cast<Let*>(expr)) {
	//	assert(false);
	//}

	if (App* app = dynamic_cast<App*>(expr)) {
		bool frL = isFreeIn(name, app->exprL);
		bool frR = isFreeIn(name, app->exprR);
		Var* vR = dynamic_cast<Var*>(app->exprR);
		CExpr* temp;
		
		if (frL && frR) {
			app->exprL = RemoveVariable(name, names, app->exprL);
			app->exprR = RemoveVariable(name, names, app->exprR);
			temp = new App(new App(new Var(Prefix, "ap"), app->exprL), app->exprR);
			app->exprL = nullptr; app->exprR = nullptr; delete app;
			return temp;
		} else if (frL) {
			app->exprL = RemoveVariable(name, names, app->exprL);
			temp = new App(new App(new Var(Prefix, "flip"), app->exprL), app->exprR);
			app->exprL = nullptr; app->exprR = nullptr; delete app;
			return temp;
		} else if (vR && vR->name == name) {
			// can possibly delete exprR
			return app->exprL;
		} else if (frR) {
			app->exprR = RemoveVariable(name, names, app->exprR);
			temp = new App(new App(new Var(Infix, "."), app->exprL), app->exprR);
			app->exprL = nullptr; app->exprR = nullptr; delete app;
			return temp;
		} else {
			return new App(new Var(Prefix, "const"), app);
		}
	}
	
	return nullptr;
}

CExpr* TransformRecursive(CExpr* expr, std::vector<std::string> names) {
	//if (Let* let = dynamic_cast<Let*>(expr)) {
	//	assert(false);
	//}

	if (Var* var = dynamic_cast<Var*>(expr)) {
		return expr;
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

		if (PTuple* pTup = dynamic_cast<PTuple*>(lambda->pat)) {
			std::string newName = fresh(names);
			CExpr* f = new App(new Var(Prefix, "fst"), new Var(Prefix, newName));
			CExpr* s = new App(new Var(Prefix, "snd"), new Var(Prefix, newName));
			CLambda* temp = new CLambda(new PVar(newName), new App(new CLambda(pTup->pat1, new CLambda(pTup->pat2, lambda->expr)), new App(f, s)));

			// NEED TO BE SURE VARIABLES ARE CLEANED APPROPRIATELY
			lambda->pat = nullptr;
			lambda->expr = nullptr;
			delete lambda;
			return temp;
		}

		if (PCons* pCons = dynamic_cast<PCons*>(lambda->pat)) {
			std::string newName = fresh(names);
			CExpr* f = new App(new Var(Prefix, "head"), new Var(Prefix, newName));
			CExpr* s = new App(new Var(Prefix, "tail"), new Var(Prefix, newName));
			CLambda* temp = new CLambda(new PVar(newName), new App(new CLambda(pCons->pat1, new CLambda(pCons->pat2, lambda->expr)), new App(f, s)));
			
			// NEED TO BE SURE VARIABLES ARE CLEANED APPROPRIATELY
			lambda->pat = nullptr;
			lambda->expr = nullptr;
			delete lambda;
			return temp;
		}
	}
	
	return nullptr;
}

CExpr* Transform(CExpr* expr) {
	std::vector<std::string> nameList;
	gatherNames(expr, nameList);
	return TransformRecursive(expr, nameList);
}

CExpr* PointFree(CExpr* expr) {
	AlphaRename(expr);
	return Transform(expr);
}
