#include "transform.hpp"

#include <algorithm>
#include <stdexcept>

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