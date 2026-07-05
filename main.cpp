#include "fol.hpp"
#include "transform.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <iostream>
#include <set>
#include <string>
#include <utility>

using namespace std;

extern int yyparse();
extern FILE *yyin;
extern Formula parsed_formula;

using Clock = chrono::high_resolution_clock;

struct NFStats
{
  size_t clauses = 0;
  size_t literals = 0;
  size_t maxClauseSize = 0;
  size_t auxAtoms = 0;
  double milliseconds = 0.0;
};

static void printUsage(const char *programName)
{
  cerr << "Upotreba:\n";
  cerr << "  " << programName << " <formula.txt>\n";
  cerr << "  echo \"formula;\" | " << programName << "\n";
}

static bool isTseitinAtom(const Formula &atom)
{
  return atom->getType() == BaseFormula::T_ATOM &&
         atom->getSymbol().rfind("__t", 0) == 0;
}

static NFStats calculateStats(const NormalForm &nf, double milliseconds)
{
  NFStats stats;
  stats.clauses = nf.size();
  stats.milliseconds = milliseconds;

  set<string> aux;

  for (const Clause &clause : nf)
  {
    stats.literals += clause.size();

    if (clause.size() > stats.maxClauseSize)
      stats.maxClauseSize = clause.size();

    for (const Literal &literal : clause)
    {
      if (isTseitinAtom(literal.atom))
        aux.insert(literal.atom->getSymbol());
    }
  }

  stats.auxAtoms = aux.size();

  return stats;
}

static void printStats(const NFStats &stats)
{
  cout << "Statistika:" << endl;
  cout << "  broj klauza: " << stats.clauses << endl;
  cout << "  broj literala: " << stats.literals << endl;
  cout << "  maksimalna duzina klauze: " << stats.maxClauseSize << endl;
  cout << "  pomocni Tseitin atomi: " << stats.auxAtoms << endl;
  cout << "  vreme transformacije: " << stats.milliseconds << " ms" << endl;
}

template <typename Func>
static pair<NormalForm, double> measure(Func transform)
{
  auto start = Clock::now();
  NormalForm nf = transform();
  auto end = Clock::now();

  chrono::duration<double, milli> diff = end - start;
  return {nf, diff.count()};
}

int main(int argc, char **argv)
{
  if (argc > 2)
  {
    printUsage(argv[0]);
    return 1;
  }

  FILE *input = stdin;

  if (argc == 2)
  {
    input = fopen(argv[1], "r");

    if (input == nullptr)
    {
      cerr << "Greska: ne mogu da otvorim fajl: " << argv[1] << endl;
      return 1;
    }
  }

  yyin = input;
  parsed_formula = nullptr;

  int parseResult = yyparse();

  if (argc == 2)
    fclose(input);

  if (parseResult != 0 || parsed_formula == nullptr)
  {
    cerr << "Greska: formula nije uspesno parsirana." << endl;
    return 1;
  }

  cout << "Parsirana formula:" << endl;
  cout << parsed_formula << endl
       << endl;

  Formula simplified = simplify(parsed_formula);
  Formula nnf = toNNF(simplified);

  cout << "Pojednostavljena formula:" << endl;
  cout << simplified << endl
       << endl;

  cout << "NNF:" << endl;
  cout << nnf << endl
       << endl;

  try
  {
    auto [classical, classicalTime] = measure([&]()
                                              { return folClassicalCNF(parsed_formula); });

    cout << "Klasicna klauzalna forma:" << endl;
    printNormalForm(classical);
    printStats(calculateStats(classical, classicalTime));
    cout << endl;
  }
  catch (const exception &e)
  {
    cout << "Klasicna klauzalna forma nije primenljiva: " << e.what() << endl
         << endl;
  }

  try
  {
    auto [tseitin, tseitinTime] = measure([&]()
                                          { return tseitinCNF(parsed_formula); });

    cout << "Tseitin KNF (iskazna logika):" << endl;
    printNormalForm(tseitin);
    printStats(calculateStats(tseitin, tseitinTime));
    cout << endl;
  }
  catch (const exception &e)
  {
    cout << "Tseitin transformacija nije primenljiva: " << e.what() << endl
         << endl;
  }

  return 0;
}