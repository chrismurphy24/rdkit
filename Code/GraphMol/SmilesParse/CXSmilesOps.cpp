//
//  Copyright (C) 2016 Greg Landrum
//
//   @@ All Rights Reserved @@
//  This file is part of the RDKit.
//  The contents are covered by the terms of the BSD license
//  which is included in the file license.txt, found at the root
//  of the RDKit source tree.
//
#include <RDGeneral/BoostStartInclude.h>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <RDGeneral/BoostEndInclude.h>
#include <GraphMol/RDKitBase.h>
#include <GraphMol/RDKitQueries.h>
#include <iostream>
#include "SmilesParse.h"
#include "SmilesParseOps.h"

namespace SmilesParseOps {
  using namespace RDKit;

  namespace parser {
    template <typename Iterator>
    std::string read_text_to(Iterator &first, Iterator last, const char sep, const char blockend) {
      Iterator start = first; 
      // EFF: there are certainly faster ways to do this
      while (first != last && *first != sep && *first != blockend) {
        ++first;
      }
      std::string res(start, first);
      return res;
    }

    template <typename Iterator>
    bool parse_atom_labels(Iterator &first, Iterator last, RDKit::RWMol &mol) {
      if (first >= last || *first != '$') return false;
      ++first;
      unsigned int atIdx = 0;
      while (first != last && *first != '$') {
        std::string tkn = read_text_to(first, last, ';', '$');
        if (tkn != "") {
          mol.getAtomWithIdx(atIdx)->setProp("_atomLabel", tkn);
        }
        ++atIdx;
        if(first != last && *first!='$') ++first;
      }
      if (first == last || *first != '$') return false;
      ++first;
      return true;
    }

    template <typename Iterator>
    bool parse_coords(Iterator &first, Iterator last, RDKit::RWMol &mol) {
      if (first >= last || *first != '(') return false;

      RDKit::Conformer *conf = new Conformer(mol.getNumAtoms());
      mol.addConformer(conf);
      ++first;
      unsigned int atIdx = 0;
      while (first != last && *first != ')') {
        RDGeom::Point3D pt;
        std::string tkn = read_text_to(first, last, ';', ')');
        if (tkn != "") {
          std::vector<std::string> tokens;
          boost::split(tokens, tkn, boost::is_any_of(std::string(",")));
          if(tokens.size()>=1 && tokens[0].size())
             pt.x = boost::lexical_cast<double>(tokens[0]);
          if (tokens.size() >= 2 && tokens[1].size())
            pt.y = boost::lexical_cast<double>(tokens[1]);
          if (tokens.size() >= 3 && tokens[2].size())
            pt.z = boost::lexical_cast<double>(tokens[2]);
        }

        conf->setAtomPos(atIdx, pt);
        ++atIdx;
        if (first != last && *first != ')') ++first;
      }
      if (first == last || *first != ')') return false;
      ++first;
      return true;

    }

    template <typename Iterator>
    bool read_int(Iterator &first, Iterator last, unsigned int &res) {
      std::string num = "";
      while (first != last && *first >= '0' && *first <= '9') {
        num += *first;
        ++first;
      }
      if (num=="") {
        return false;
      }
      res = boost::lexical_cast<unsigned int>(num);
      return true;
  }
    template <typename Iterator>
    bool read_int_pair(Iterator &first, Iterator last, unsigned int &n1, unsigned int &n2, char sep='.') {
      if (!read_int(first, last, n1)) return false;
      if (first >= last || *first != sep) return false;
      ++first;
      return read_int(first, last, n2);
    }
    
    template <typename Iterator>
  bool parse_coordinate_bonds(Iterator &first, Iterator last, RDKit::RWMol &mol) {
    if (first >= last || *first != 'C') return false;
    ++first;
    if (first >= last || *first != ':') return false;
    ++first;
    while (first != last && *first >= '0' && *first<='9') {
      unsigned int aidx;
      unsigned int bidx;
      if (read_int_pair(first, last, aidx,bidx)) {
        Bond *bnd = mol.getBondWithIdx(bidx);
        if (bnd->getBeginAtomIdx() != aidx && bnd->getEndAtomIdx() != aidx) {
          std::cerr << "BOND NOT FOUND! " << bidx << " involving atom " << aidx <<  std::endl;
          return false;
        }
        bnd->setBondType(Bond::DATIVE);
        if (bnd->getBeginAtomIdx() != aidx) {
          unsigned int tmp = bnd->getBeginAtomIdx();
          bnd->setBeginAtomIdx(aidx);
          bnd->setEndAtomIdx(tmp);
        }
      }
      else {
        return false;
      }
      if (first < last && *first == ',') ++first;
    }
    return true;
  }

  template <typename Iterator>
  bool processRadicalSection(Iterator &first, Iterator last, RDKit::RWMol &mol,
    unsigned int numRadicalElectrons) {
    if (first >= last ) return false;
    ++first;
    if (first >= last || *first != ':') return false;
    ++first;
    unsigned int atIdx;
    if (!read_int(first, last, atIdx)) return false;
    mol.getAtomWithIdx(atIdx)->setNumRadicalElectrons(numRadicalElectrons);
    while (first < last && *first == ',') {
      ++first;
      if (first < last && (*first < '0' || *first>'9')) return true;
      if (!read_int(first, last, atIdx)) return false;
      mol.getAtomWithIdx(atIdx)->setNumRadicalElectrons(numRadicalElectrons);
    }
    if (first >= last) return false;
    return true;
  }

  template <typename Iterator>
  bool parse_radicals(Iterator &first, Iterator last, RDKit::RWMol &mol) {
    if (first >= last || *first != '^') return false;
    while (*first == '^') {
      ++first;
      if (first >= last ) return false;
      if (*first<'1' || *first>'7') return false; // these are the values that are allowed to be there
      switch (*first) {
      case '1':
        if(!processRadicalSection(first, last, mol, 1)) return false;
        break;
      case '2':
      case '3':
      case '4':
        if (!processRadicalSection(first, last, mol, 2)) return false;
        break;
      case '5':
      case '6':
      case '7':
        if (!processRadicalSection(first, last, mol, 3)) return false;
        break;
      default:
        BOOST_LOG(rdWarningLog) << "Radical specification " << *first << " ignored.";
      }
    }
    return true;
  }

    template <typename Iterator>
    bool parse_it(Iterator &first, Iterator last,RDKit::RWMol &mol) {
      if (first >= last || *first != '|') return false;
      ++first;
      while (first < last && *first != '|') {
        if (*first == '(') {
          if (!parse_coords(first, last, mol)) return false;
        }
        else if (*first == '$') {
          if (!parse_atom_labels(first, last, mol)) return false;
        }
        else if (*first == 'C') {
          if (!parse_coordinate_bonds(first, last, mol)) return false;
        }
        else if (*first == '^') {
          if (!parse_radicals(first, last, mol)) return false;
        }
        //if(first < last && *first != '|') ++first;
      }
      if (first >= last || *first != '|') return false;
      ++first; // step past the last '|'
      return true;
    }
  } // end of namespace parser

  namespace {
    void processCXSmilesLabels(RDKit::RWMol &mol) {
      for (RDKit::ROMol::AtomIterator atIt = mol.beginAtoms(); atIt != mol.endAtoms(); ++atIt) {
        std::string symb = "";
        if((*atIt)->getPropIfPresent("_atomLabel",symb)) {
          if (symb == "AH_p" || symb == "QH_p" || symb == "XH_p" || symb == "X_p" ||
            symb == "Q_e" || symb == "star_e") {
            QueryAtom *query = new QueryAtom(0);
            if (symb == "star_e") {
              /* according to the MDL spec, these match anything, but in MARVIN they are
              "unspecified end groups" for polymers */
              query->setQuery(makeAtomNullQuery());
            } else if (symb == "Q_e") {
              ATOM_OR_QUERY *q = new ATOM_OR_QUERY;
              q->setDescription("AtomOr");
              q->setNegation(true);
              q->addChild(
                QueryAtom::QUERYATOM_QUERY::CHILD_TYPE(makeAtomNumQuery(6)));
              q->addChild(
                QueryAtom::QUERYATOM_QUERY::CHILD_TYPE(makeAtomNumQuery(1)));
              query->setQuery(q);
            } else if (symb == "QH_p") {
              ATOM_EQUALS_QUERY *q = makeAtomNumQuery(6);
              q->setNegation(true);
              query->setQuery(q);
            } else if (symb == "AH_p") { // this seems wrong...
              /* According to the MARVIN Sketch, AH is "any atom, including H" - this would be 
              "*" in SMILES - and "A" is "any atom except H".
              The CXSMILES docs say that "A" can be represented normally in SMILES and that "AH" 
              needs to be written out as AH_p. I'm not sure what to make of this.*/
              ATOM_EQUALS_QUERY *q = makeAtomNumQuery(1);
              q->setNegation(true);
              query->setQuery(q);
            } else if (symb == "X_p" || symb == "XH_p") { 
              ATOM_OR_QUERY *q = new ATOM_OR_QUERY;
              q->setDescription("AtomOr");
              q->addChild(
                QueryAtom::QUERYATOM_QUERY::CHILD_TYPE(makeAtomNumQuery(9)));
              q->addChild(
                QueryAtom::QUERYATOM_QUERY::CHILD_TYPE(makeAtomNumQuery(17)));
              q->addChild(
                QueryAtom::QUERYATOM_QUERY::CHILD_TYPE(makeAtomNumQuery(35)));
              q->addChild(
                QueryAtom::QUERYATOM_QUERY::CHILD_TYPE(makeAtomNumQuery(53)));
              q->addChild(
                QueryAtom::QUERYATOM_QUERY::CHILD_TYPE(makeAtomNumQuery(85)));
              if (symb == "XH_p") {
                q->addChild(
                  QueryAtom::QUERYATOM_QUERY::CHILD_TYPE(makeAtomNumQuery(1)));
              }
              query->setQuery(q);
            }
            // queries have no implicit Hs:
            query->setNoImplicit(true);
            mol.replaceAtom((*atIt)->getIdx(), query);
            mol.getAtomWithIdx((*atIt)->getIdx())->setProp("_atomLabel", symb);
          }
        }
      }
    }

  } // end of anonymous namespace


  void parseCXExtensions(RDKit::RWMol & mol, const std::string & extText, std::string::const_iterator &first) {
    //std::cerr << "parseCXNExtensions: " << extText << std::endl;
    if (!extText.size() || extText[0] != '|') return;
    first = extText.begin();
    bool ok = parser::parse_it(first, extText.end(),mol);
    if (!ok) throw RDKit::SmilesParseException("failure parsing CXSMILES extensions");
    if (ok) {
      processCXSmilesLabels(mol);
    }
  }
} // end of namespace SmilesParseOps