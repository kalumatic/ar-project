#include "fol.hpp"
#include "transform.hpp"

#include <cstdio>
#include <iostream>
#include <stdexcept>

using namespace std;

extern int yyparse();
extern FILE *yyin;
extern Formula parsed_formula;

static void printUsage(const char *programName)
{
  cerr << "Upotreba:\n";
  cerr << "  " << programName << " <formula.txt>\n";
  cerr << "  echo \"formula;\" | " << programName << "\n";
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
  {
    fclose(input);
  }

  if (parseResult != 0 || parsed_formula == nullptr)
  {
    cerr << "Greska: formula nije uspesno parsirana." << endl;
    return 1;
  }

  cout << "Parsirana formula:" << endl;
  cout << parsed_formula << endl
       << endl;

  Formula nnf = toNNF(parsed_formula);

  cout << "NNF:" << endl;
  cout << nnf << endl
       << endl;

  try
  {
    NormalForm classical = classicalCNF(parsed_formula);

    cout << "Klasicna KNF:" << endl;
    printNormalForm(classical);
    cout << endl;
  }
  catch (const exception &e)
  {
    cout << "Klasicna KNF nije primenljiva: " << e.what() << endl
         << endl;
  }

  try
  {
    NormalForm tseitin = tseitinCNF(parsed_formula);

    cout << "Tseitin KNF:" << endl;
    printNormalForm(tseitin);
    cout << endl;
  }
  catch (const exception &e)
  {
    cout << "Tseitin transformacija nije primenljiva: " << e.what() << endl
         << endl;
  }

  return 0;
}