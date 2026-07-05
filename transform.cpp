#include "transform.hpp"

#include <algorithm>
#include <stdexcept>
#include <map>
#include <set>
#include <vector>

using namespace std;

static FormulaDatabase &FDB()
{
    return FormulaDatabase::getFormulaDatabase();
}

template <typename List>
static List concat(const List &l, const List &r)
{
    List result;
    copy(begin(l), end(l), back_inserter(result));
    copy(begin(r), end(r), back_inserter(result));
    return result;
}

static NormalForm cross(const NormalForm &l, const NormalForm &r)
{
    NormalForm result;

    for (const auto &lc : l)
    {
        for (const auto &rc : r)
        {
            result.push_back(concat(lc, rc));
        }
    }

    return result;
}

Formula toNNFNot(const Formula &f)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
        return FDB().makeFalse();

    case BaseFormula::T_FALSE:
        return FDB().makeTrue();

    case BaseFormula::T_ATOM:
        return FDB().makeNot(f);

    case BaseFormula::T_NOT:
        return toNNF(f->getOperand());

    case BaseFormula::T_AND:
        return FDB().makeOr(
            toNNFNot(f->getOperand1()),
            toNNFNot(f->getOperand2()));

    case BaseFormula::T_OR:
        return FDB().makeAnd(
            toNNFNot(f->getOperand1()),
            toNNFNot(f->getOperand2()));

    case BaseFormula::T_IMP:
        return FDB().makeAnd(
            toNNF(f->getOperand1()),
            toNNFNot(f->getOperand2()));

    case BaseFormula::T_IFF:
        return FDB().makeOr(
            FDB().makeAnd(
                toNNF(f->getOperand1()),
                toNNFNot(f->getOperand2())),
            FDB().makeAnd(
                toNNFNot(f->getOperand1()),
                toNNF(f->getOperand2())));

    case BaseFormula::T_FORALL:
        return FDB().makeExists(
            f->getVariable(),
            toNNFNot(f->getOperand()));

    case BaseFormula::T_EXISTS:
        return FDB().makeForall(
            f->getVariable(),
            toNNFNot(f->getOperand()));
    }

    throw runtime_error("Nepoznat tip formule u toNNFNot");
}

Formula toNNF(const Formula &f)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
    case BaseFormula::T_ATOM:
        return f;

    case BaseFormula::T_NOT:
        return toNNFNot(f->getOperand());

    case BaseFormula::T_AND:
        return FDB().makeAnd(
            toNNF(f->getOperand1()),
            toNNF(f->getOperand2()));

    case BaseFormula::T_OR:
        return FDB().makeOr(
            toNNF(f->getOperand1()),
            toNNF(f->getOperand2()));

    case BaseFormula::T_IMP:
        return FDB().makeOr(
            toNNFNot(f->getOperand1()),
            toNNF(f->getOperand2()));

    case BaseFormula::T_IFF:
        return FDB().makeAnd(
            FDB().makeOr(
                toNNFNot(f->getOperand1()),
                toNNF(f->getOperand2())),
            FDB().makeOr(
                toNNF(f->getOperand1()),
                toNNFNot(f->getOperand2())));

    case BaseFormula::T_FORALL:
        return FDB().makeForall(
            f->getVariable(),
            toNNF(f->getOperand()));

    case BaseFormula::T_EXISTS:
        return FDB().makeExists(
            f->getVariable(),
            toNNF(f->getOperand()));
    }

    throw runtime_error("Nepoznat tip formule u toNNF");
}

NormalForm toCNF(const Formula &f)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
        return {};

    case BaseFormula::T_FALSE:
        return {{}};

    case BaseFormula::T_ATOM:
        return {{Literal{true, f}}};

    case BaseFormula::T_NOT:
    {
        Formula op = f->getOperand();

        if (op->getType() != BaseFormula::T_ATOM)
        {
            throw runtime_error("toCNF ocekuje NNF formulu: negacija nije ispred atoma");
        }

        return {{Literal{false, op}}};
    }

    case BaseFormula::T_AND:
        return concat(
            toCNF(f->getOperand1()),
            toCNF(f->getOperand2()));

    case BaseFormula::T_OR:
        return cross(
            toCNF(f->getOperand1()),
            toCNF(f->getOperand2()));

    case BaseFormula::T_IMP:
    case BaseFormula::T_IFF:
        throw runtime_error("toCNF ocekuje formulu bez implikacija i ekvivalencija");

    case BaseFormula::T_FORALL:
    case BaseFormula::T_EXISTS:
        throw runtime_error("toCNF za sada ocekuje formulu bez kvantifikatora");
    }

    throw runtime_error("Nepoznat tip formule u toCNF");
}

NormalForm classicalCNF(const Formula &f)
{
    Formula nnf = toNNF(f);
    return toCNF(nnf);
}

static Literal neg(const Literal &l)
{
    return Literal{!l.pos, l.atom};
}

static Formula makeFreshAtom(int &counter)
{
    return FDB().makeAtom("__t" + to_string(++counter));
}

static Literal tseitinRec(const Formula &f, int &counter, NormalForm &cnf)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    {
        Formula a = makeFreshAtom(counter);
        Literal l{true, a};
        cnf.push_back({l});
        return l;
    }

    case BaseFormula::T_FALSE:
    {
        Formula a = makeFreshAtom(counter);
        Literal l{true, a};
        cnf.push_back({neg(l)});
        return l;
    }

    case BaseFormula::T_ATOM:
        return Literal{true, f};

    case BaseFormula::T_NOT:
    {
        Formula op = f->getOperand();

        if (op->getType() != BaseFormula::T_ATOM)
        {
            throw runtime_error("Tseitin ocekuje NNF formulu");
        }

        return Literal{false, op};
    }

    case BaseFormula::T_AND:
    {
        Literal l = tseitinRec(f->getOperand1(), counter, cnf);
        Literal r = tseitinRec(f->getOperand2(), counter, cnf);

        Formula a = makeFreshAtom(counter);
        Literal t{true, a};

        cnf.push_back({neg(t), l});
        cnf.push_back({neg(t), r});
        cnf.push_back({t, neg(l), neg(r)});

        return t;
    }

    case BaseFormula::T_OR:
    {
        Literal l = tseitinRec(f->getOperand1(), counter, cnf);
        Literal r = tseitinRec(f->getOperand2(), counter, cnf);

        Formula a = makeFreshAtom(counter);
        Literal t{true, a};

        cnf.push_back({t, neg(l)});
        cnf.push_back({t, neg(r)});
        cnf.push_back({neg(t), l, r});

        return t;
    }

    case BaseFormula::T_IMP:
    case BaseFormula::T_IFF:
        throw runtime_error("Tseitin ocekuje formulu bez => i <=>, prvo pozovi toNNF");

    case BaseFormula::T_FORALL:
    case BaseFormula::T_EXISTS:
        throw runtime_error("Tseitin transformacija je trenutno samo za iskazne formule");
    }

    throw runtime_error("Nepoznat tip formule u tseitinRec");
}

NormalForm tseitinCNF(const Formula &f)
{
    Formula nnf = toNNF(f);

    NormalForm cnf;
    int counter = 0;

    Literal top = tseitinRec(nnf, counter, cnf);

    cnf.push_back({top});

    return cnf;
}

static void collectTermVariables(const Term &t, set<Variable> &vars)
{
    if (t->getType() == BaseTerm::TT_VARIABLE)
    {
        vars.insert(t->getVariable());
        return;
    }

    for (const Term &op : t->getOperands())
    {
        collectTermVariables(op, vars);
    }
}

static void collectFunctionSymbols(const Term &t, set<FunctionSymbol> &symbols)
{
    if (t->getType() == BaseTerm::TT_FUNCTION)
    {
        symbols.insert(t->getSymbol());

        for (const Term &op : t->getOperands())
        {
            collectFunctionSymbols(op, symbols);
        }
    }
}

static void collectFunctionSymbols(const Formula &f, set<FunctionSymbol> &symbols)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
        return;

    case BaseFormula::T_ATOM:
        for (const Term &t : f->getOperands())
        {
            collectFunctionSymbols(t, symbols);
        }
        return;

    case BaseFormula::T_NOT:
        collectFunctionSymbols(f->getOperand(), symbols);
        return;

    case BaseFormula::T_AND:
    case BaseFormula::T_OR:
    case BaseFormula::T_IMP:
    case BaseFormula::T_IFF:
        collectFunctionSymbols(f->getOperand1(), symbols);
        collectFunctionSymbols(f->getOperand2(), symbols);
        return;

    case BaseFormula::T_FORALL:
    case BaseFormula::T_EXISTS:
        collectFunctionSymbols(f->getOperand(), symbols);
        return;
    }
}

static void collectAllVariables(const Formula &f, set<Variable> &vars)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
        return;

    case BaseFormula::T_ATOM:
        for (const Term &t : f->getOperands())
        {
            collectTermVariables(t, vars);
        }
        return;

    case BaseFormula::T_NOT:
        collectAllVariables(f->getOperand(), vars);
        return;

    case BaseFormula::T_AND:
    case BaseFormula::T_OR:
    case BaseFormula::T_IMP:
    case BaseFormula::T_IFF:
        collectAllVariables(f->getOperand1(), vars);
        collectAllVariables(f->getOperand2(), vars);
        return;

    case BaseFormula::T_FORALL:
    case BaseFormula::T_EXISTS:
        vars.insert(f->getVariable());
        collectAllVariables(f->getOperand(), vars);
        return;
    }
}

static void collectFreeVariables(const Formula &f, set<Variable> &freeVars, set<Variable> &bound)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
        return;

    case BaseFormula::T_ATOM:
        for (const Term &t : f->getOperands())
        {
            set<Variable> termVars;
            collectTermVariables(t, termVars);

            for (const Variable &v : termVars)
            {
                if (bound.find(v) == bound.end())
                {
                    freeVars.insert(v);
                }
            }
        }
        return;

    case BaseFormula::T_NOT:
        collectFreeVariables(f->getOperand(), freeVars, bound);
        return;

    case BaseFormula::T_AND:
    case BaseFormula::T_OR:
    case BaseFormula::T_IMP:
    case BaseFormula::T_IFF:
        collectFreeVariables(f->getOperand1(), freeVars, bound);
        collectFreeVariables(f->getOperand2(), freeVars, bound);
        return;

    case BaseFormula::T_FORALL:
    case BaseFormula::T_EXISTS:
    {
        bound.insert(f->getVariable());
        collectFreeVariables(f->getOperand(), freeVars, bound);
        bound.erase(f->getVariable());
        return;
    }
    }
}

static bool containsFreeVariable(const Formula &f, const Variable &v)
{
    set<Variable> freeVars;
    set<Variable> bound;
    collectFreeVariables(f, freeVars, bound);
    return freeVars.find(v) != freeVars.end();
}

static string freshVariableName(set<Variable> &used, int &counter)
{
    string name;

    do
    {
        name = "V" + to_string(++counter);
    } while (used.find(name) != used.end());

    used.insert(name);
    return name;
}

static string freshSkolemName(set<FunctionSymbol> &used, int &counter)
{
    string name;

    do
    {
        name = "sk" + to_string(++counter);
    } while (used.find(name) != used.end());

    used.insert(name);
    return name;
}

static Term substituteTerm(const Term &t, const Variable &v, const Term &replacement)
{
    if (t->getType() == BaseTerm::TT_VARIABLE)
    {
        if (t->getVariable() == v)
        {
            return replacement;
        }

        return t;
    }

    vector<Term> newOps;

    for (const Term &op : t->getOperands())
    {
        newOps.push_back(substituteTerm(op, v, replacement));
    }

    return TermDatabase::getTermDatabase().makeFunctionTerm(t->getSymbol(), newOps);
}

static Formula substituteFormula(const Formula &f, const Variable &v, const Term &replacement)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
        return f;

    case BaseFormula::T_ATOM:
    {
        vector<Term> newOps;

        for (const Term &t : f->getOperands())
        {
            newOps.push_back(substituteTerm(t, v, replacement));
        }

        if (f->getSymbol() == "=" && newOps.size() == 2)
        {
            return FDB().makeEquality(newOps[0], newOps[1]);
        }

        if (f->getSymbol() == "~=" && newOps.size() == 2)
        {
            return FDB().makeDisequality(newOps[0], newOps[1]);
        }

        return FDB().makeAtom(f->getSymbol(), newOps);
    }

    case BaseFormula::T_NOT:
        return FDB().makeNot(substituteFormula(f->getOperand(), v, replacement));

    case BaseFormula::T_AND:
        return FDB().makeAnd(
            substituteFormula(f->getOperand1(), v, replacement),
            substituteFormula(f->getOperand2(), v, replacement));

    case BaseFormula::T_OR:
        return FDB().makeOr(
            substituteFormula(f->getOperand1(), v, replacement),
            substituteFormula(f->getOperand2(), v, replacement));

    case BaseFormula::T_IMP:
        return FDB().makeImp(
            substituteFormula(f->getOperand1(), v, replacement),
            substituteFormula(f->getOperand2(), v, replacement));

    case BaseFormula::T_IFF:
        return FDB().makeIff(
            substituteFormula(f->getOperand1(), v, replacement),
            substituteFormula(f->getOperand2(), v, replacement));

    case BaseFormula::T_FORALL:
        if (f->getVariable() == v)
        {
            return f;
        }

        return FDB().makeForall(
            f->getVariable(),
            substituteFormula(f->getOperand(), v, replacement));

    case BaseFormula::T_EXISTS:
        if (f->getVariable() == v)
        {
            return f;
        }

        return FDB().makeExists(
            f->getVariable(),
            substituteFormula(f->getOperand(), v, replacement));
    }

    throw runtime_error("Nepoznat tip formule u substituteFormula");
}

Formula simplify(const Formula &f)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
    case BaseFormula::T_ATOM:
        return f;

    case BaseFormula::T_NOT:
    {
        Formula op = simplify(f->getOperand());

        if (op->getType() == BaseFormula::T_TRUE)
        {
            return FDB().makeFalse();
        }

        if (op->getType() == BaseFormula::T_FALSE)
        {
            return FDB().makeTrue();
        }

        if (op->getType() == BaseFormula::T_NOT)
        {
            return simplify(op->getOperand());
        }

        return FDB().makeNot(op);
    }

    case BaseFormula::T_AND:
    {
        Formula l = simplify(f->getOperand1());
        Formula r = simplify(f->getOperand2());

        if (l->getType() == BaseFormula::T_FALSE || r->getType() == BaseFormula::T_FALSE)
        {
            return FDB().makeFalse();
        }

        if (l->getType() == BaseFormula::T_TRUE)
        {
            return r;
        }

        if (r->getType() == BaseFormula::T_TRUE)
        {
            return l;
        }

        return FDB().makeAnd(l, r);
    }

    case BaseFormula::T_OR:
    {
        Formula l = simplify(f->getOperand1());
        Formula r = simplify(f->getOperand2());

        if (l->getType() == BaseFormula::T_TRUE || r->getType() == BaseFormula::T_TRUE)
        {
            return FDB().makeTrue();
        }

        if (l->getType() == BaseFormula::T_FALSE)
        {
            return r;
        }

        if (r->getType() == BaseFormula::T_FALSE)
        {
            return l;
        }

        return FDB().makeOr(l, r);
    }

    case BaseFormula::T_IMP:
    {
        Formula l = simplify(f->getOperand1());
        Formula r = simplify(f->getOperand2());

        if (l->getType() == BaseFormula::T_FALSE || r->getType() == BaseFormula::T_TRUE)
        {
            return FDB().makeTrue();
        }

        if (l->getType() == BaseFormula::T_TRUE)
        {
            return r;
        }

        if (r->getType() == BaseFormula::T_FALSE)
        {
            return simplify(FDB().makeNot(l));
        }

        return FDB().makeImp(l, r);
    }

    case BaseFormula::T_IFF:
    {
        Formula l = simplify(f->getOperand1());
        Formula r = simplify(f->getOperand2());

        if (l->getType() == BaseFormula::T_TRUE)
        {
            return r;
        }

        if (r->getType() == BaseFormula::T_TRUE)
        {
            return l;
        }

        if (l->getType() == BaseFormula::T_FALSE)
        {
            return simplify(FDB().makeNot(r));
        }

        if (r->getType() == BaseFormula::T_FALSE)
        {
            return simplify(FDB().makeNot(l));
        }

        return FDB().makeIff(l, r);
    }

    case BaseFormula::T_FORALL:
    {
        Formula op = simplify(f->getOperand());

        if (!containsFreeVariable(op, f->getVariable()))
        {
            return op;
        }

        return FDB().makeForall(f->getVariable(), op);
    }

    case BaseFormula::T_EXISTS:
    {
        Formula op = simplify(f->getOperand());

        if (!containsFreeVariable(op, f->getVariable()))
        {
            return op;
        }

        return FDB().makeExists(f->getVariable(), op);
    }
    }

    throw runtime_error("Nepoznat tip formule u simplify");
}

static Term standardizeTerm(const Term &t, const map<Variable, Variable> &env)
{
    if (t->getType() == BaseTerm::TT_VARIABLE)
    {
        auto it = env.find(t->getVariable());

        if (it != env.end())
        {
            return TermDatabase::getTermDatabase().makeVariableTerm(it->second);
        }

        return t;
    }

    vector<Term> newOps;

    for (const Term &op : t->getOperands())
    {
        newOps.push_back(standardizeTerm(op, env));
    }

    return TermDatabase::getTermDatabase().makeFunctionTerm(t->getSymbol(), newOps);
}

static Formula standardizeRec(const Formula &f, map<Variable, Variable> &env, set<Variable> &used, int &counter)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
        return f;

    case BaseFormula::T_ATOM:
    {
        vector<Term> newOps;

        for (const Term &t : f->getOperands())
        {
            newOps.push_back(standardizeTerm(t, env));
        }

        if (f->getSymbol() == "=" && newOps.size() == 2)
        {
            return FDB().makeEquality(newOps[0], newOps[1]);
        }

        if (f->getSymbol() == "~=" && newOps.size() == 2)
        {
            return FDB().makeDisequality(newOps[0], newOps[1]);
        }

        return FDB().makeAtom(f->getSymbol(), newOps);
    }

    case BaseFormula::T_NOT:
        return FDB().makeNot(standardizeRec(f->getOperand(), env, used, counter));

    case BaseFormula::T_AND:
        return FDB().makeAnd(
            standardizeRec(f->getOperand1(), env, used, counter),
            standardizeRec(f->getOperand2(), env, used, counter));

    case BaseFormula::T_OR:
        return FDB().makeOr(
            standardizeRec(f->getOperand1(), env, used, counter),
            standardizeRec(f->getOperand2(), env, used, counter));

    case BaseFormula::T_IMP:
        return FDB().makeImp(
            standardizeRec(f->getOperand1(), env, used, counter),
            standardizeRec(f->getOperand2(), env, used, counter));

    case BaseFormula::T_IFF:
        return FDB().makeIff(
            standardizeRec(f->getOperand1(), env, used, counter),
            standardizeRec(f->getOperand2(), env, used, counter));

    case BaseFormula::T_FORALL:
    case BaseFormula::T_EXISTS:
    {
        Variable oldVar = f->getVariable();
        Variable newVar = freshVariableName(used, counter);

        bool hadOld = env.find(oldVar) != env.end();
        Variable previous;

        if (hadOld)
        {
            previous = env[oldVar];
        }

        env[oldVar] = newVar;

        Formula sub = standardizeRec(f->getOperand(), env, used, counter);

        if (hadOld)
        {
            env[oldVar] = previous;
        }
        else
        {
            env.erase(oldVar);
        }

        if (f->getType() == BaseFormula::T_FORALL)
        {
            return FDB().makeForall(newVar, sub);
        }

        return FDB().makeExists(newVar, sub);
    }
    }

    throw runtime_error("Nepoznat tip formule u standardizeRec");
}

Formula standardizeVariables(const Formula &f)
{
    set<Variable> used;
    collectAllVariables(f, used);

    map<Variable, Variable> env;
    int counter = 0;

    return standardizeRec(f, env, used, counter);
}

struct PrenexQuantifier
{
    bool universal;
    Variable var;
};

struct PrenexResult
{
    vector<PrenexQuantifier> prefix;
    Formula matrix;
};

static PrenexResult prenexRec(const Formula &f)
{
    switch (f->getType())
    {
    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
    case BaseFormula::T_ATOM:
    case BaseFormula::T_NOT:
        return {{}, f};

    case BaseFormula::T_AND:
    case BaseFormula::T_OR:
    {
        PrenexResult l = prenexRec(f->getOperand1());
        PrenexResult r = prenexRec(f->getOperand2());

        vector<PrenexQuantifier> prefix;
        prefix.insert(prefix.end(), l.prefix.begin(), l.prefix.end());
        prefix.insert(prefix.end(), r.prefix.begin(), r.prefix.end());

        Formula matrix = f->getType() == BaseFormula::T_AND
                             ? FDB().makeAnd(l.matrix, r.matrix)
                             : FDB().makeOr(l.matrix, r.matrix);

        return {prefix, matrix};
    }

    case BaseFormula::T_FORALL:
    {
        PrenexResult sub = prenexRec(f->getOperand());
        sub.prefix.insert(sub.prefix.begin(), PrenexQuantifier{true, f->getVariable()});
        return sub;
    }

    case BaseFormula::T_EXISTS:
    {
        PrenexResult sub = prenexRec(f->getOperand());
        sub.prefix.insert(sub.prefix.begin(), PrenexQuantifier{false, f->getVariable()});
        return sub;
    }

    case BaseFormula::T_IMP:
    case BaseFormula::T_IFF:
        throw runtime_error("toPrenex ocekuje NNF formulu bez => i <=>");
    }

    throw runtime_error("Nepoznat tip formule u prenexRec");
}

Formula toPrenex(const Formula &f)
{
    PrenexResult result = prenexRec(f);
    Formula current = result.matrix;

    for (auto it = result.prefix.rbegin(); it != result.prefix.rend(); ++it)
    {
        if (it->universal)
        {
            current = FDB().makeForall(it->var, current);
        }
        else
        {
            current = FDB().makeExists(it->var, current);
        }
    }

    return current;
}

static Formula skolemRec(const Formula &f, vector<Variable> &universalVars, set<FunctionSymbol> &usedFunctions, int &counter)
{
    if (f->getType() != BaseFormula::T_FORALL && f->getType() != BaseFormula::T_EXISTS)
    {
        return f;
    }

    if (f->getType() == BaseFormula::T_FORALL)
    {
        universalVars.push_back(f->getVariable());
        Formula sub = skolemRec(f->getOperand(), universalVars, usedFunctions, counter);
        universalVars.pop_back();

        return FDB().makeForall(f->getVariable(), sub);
    }

    string skolemName = freshSkolemName(usedFunctions, counter);
    vector<Term> args;

    for (const Variable &v : universalVars)
    {
        args.push_back(TermDatabase::getTermDatabase().makeVariableTerm(v));
    }

    Term skolemTerm = TermDatabase::getTermDatabase().makeFunctionTerm(skolemName, args);
    Formula substituted = substituteFormula(f->getOperand(), f->getVariable(), skolemTerm);

    return skolemRec(substituted, universalVars, usedFunctions, counter);
}

Formula skolemize(const Formula &f)
{
    set<FunctionSymbol> usedFunctions;
    collectFunctionSymbols(f, usedFunctions);

    vector<Variable> universalVars;
    int counter = 0;

    return skolemRec(f, universalVars, usedFunctions, counter);
}

Formula removeUniversalQuantifiers(const Formula &f)
{
    switch (f->getType())
    {
    case BaseFormula::T_FORALL:
        return removeUniversalQuantifiers(f->getOperand());

    case BaseFormula::T_EXISTS:
        throw runtime_error("Nakon Skolemizacije ne sme ostati egzistencijalni kvantifikator");

    case BaseFormula::T_AND:
        return FDB().makeAnd(
            removeUniversalQuantifiers(f->getOperand1()),
            removeUniversalQuantifiers(f->getOperand2()));

    case BaseFormula::T_OR:
        return FDB().makeOr(
            removeUniversalQuantifiers(f->getOperand1()),
            removeUniversalQuantifiers(f->getOperand2()));

    case BaseFormula::T_NOT:
        return FDB().makeNot(removeUniversalQuantifiers(f->getOperand()));

    case BaseFormula::T_TRUE:
    case BaseFormula::T_FALSE:
    case BaseFormula::T_ATOM:
        return f;

    case BaseFormula::T_IMP:
    case BaseFormula::T_IFF:
        throw runtime_error("removeUniversalQuantifiers ocekuje formulu bez => i <=>");
    }

    throw runtime_error("Nepoznat tip formule u removeUniversalQuantifiers");
}

NormalForm folClassicalCNF(const Formula &f)
{
    Formula simplified = simplify(f);
    Formula nnf = toNNF(simplified);
    Formula simplifiedNNF = simplify(nnf);
    Formula standardized = standardizeVariables(simplifiedNNF);
    Formula prenex = toPrenex(standardized);
    Formula skolemized = skolemize(prenex);
    Formula matrix = removeUniversalQuantifiers(skolemized);

    return toCNF(matrix);
}

void printNormalForm(const NormalForm &nf, ostream &out)
{
    if (nf.empty())
    {
        out << "TRUE" << endl;
        return;
    }

    for (const auto &clause : nf)
    {
        out << "[ ";

        if (clause.empty())
        {
            out << "FALSE ";
        }

        for (const auto &literal : clause)
        {
            if (!literal.pos)
            {
                out << "~";
            }

            out << literal.atom << " ";
        }

        out << "]";
    }

    out << endl;
}