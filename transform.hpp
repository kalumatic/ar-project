#ifndef TRANSFORM_HPP
#define TRANSFORM_HPP

#include "fol.hpp"

#include <vector>
#include <iostream>

struct Literal
{
    bool pos;
    Formula atom;
};

using Clause = std::vector<Literal>;
using NormalForm = std::vector<Clause>;

Formula toNNF(const Formula &f);
Formula toNNFNot(const Formula &f);

NormalForm toCNF(const Formula &f);
NormalForm classicalCNF(const Formula &f);

void printNormalForm(const NormalForm &nf, std::ostream &out = std::cout);

#endif