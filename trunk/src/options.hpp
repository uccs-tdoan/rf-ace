#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include "argparse.hpp"
#include "datadefs.hpp"
#include "treedata.hpp"

using namespace std;
using datadefs::num_t;

namespace options {

  void printHeader(ostream& output) {

    output << endl;
    output << "-----------------------------------------------------" << endl;
    output << "|  RF-ACE version:  0.9.9, February 2nd, 2012       |" << endl;
    output << "|    Project page:  http://code.google.com/p/rf-ace |" << endl;
    output << "|     Report bugs:  timo.p.erkkila@tut.fi           |" << endl;
    output << "-----------------------------------------------------" << endl;
    output << endl;
  }

  void printHelpHint() {
    cout << endl;
    cout << "To get started, type \"-h\" or \"--help\"" << endl;
  }

  const bool   GENERAL_DEFAULT_PRINT_HELP = false;
  const char   GENERAL_DEFAULT_DATA_DELIMITER = '\t';
  const char   GENERAL_DEFAULT_HEADER_DELIMITER = ':';
  const size_t GENERAL_DEFAULT_MIN_SAMPLES = 5;
  
  const size_t RF_DEFAULT_N_TREES = 1000; // zero means it will be estimated from the data by default
  const size_t RF_DEFAULT_M_TRY = 0; // same here ...
  const size_t RF_DEFAULT_N_MAX_LEAVES = 100;
  const size_t RF_DEFAULT_NODE_SIZE = 3; // ... and here
  const size_t RF_DEFAULT_N_PERMS = 20;
  const num_t  RF_DEFAULT_P_VALUE_THRESHOLD = 0.05;
  
  const size_t GBT_DEFAULT_N_TREES = 100;
  const size_t GBT_DEFAULT_N_MAX_LEAVES = 6;
  const num_t  GBT_DEFAULT_SHRINKAGE = 0.1;
  const num_t  GBT_DEFAULT_SUB_SAMPLE_SIZE = 0.5;

  // Determines the amount of indentation in help print-outs
  const size_t maxWidth = 20;
  
  struct General_options {

    bool printHelp; const string printHelp_s; const string printHelp_l;
    string input; const string input_s; const string input_l;
    string output; const string output_s; const string output_l;
    string targetStr; const string targetStr_s; const string targetStr_l;
    string whiteList; const string whiteList_s; const string whiteList_l;
    string blackList; const string blackList_s; const string blackList_l;
    string log; const string log_s; const string log_l;
    char dataDelimiter; const string dataDelimiter_s; const string dataDelimiter_l;
    char headerDelimiter; const string headerDelimiter_s; const string headerDelimiter_l;
    size_t pruneFeatures; const string pruneFeatures_s; const string pruneFeatures_l;

    General_options(const int argc, char* const argv[]):
      
      printHelp(GENERAL_DEFAULT_PRINT_HELP),printHelp_s("h"),printHelp_l("help"),
      input(""),input_s("I"),input_l("input"),
      output(""),output_s("O"),output_l("output"),
      targetStr(""),targetStr_s("i"),targetStr_l("target"),
      whiteList(""),whiteList_s("W"),whiteList_l("whitelist"),
      blackList(""),blackList_s("B"),blackList_l("blacklist"),
      log(""),log_s("L"),log_l("log"),
      dataDelimiter(GENERAL_DEFAULT_DATA_DELIMITER),dataDelimiter_s("D"),dataDelimiter_l("data_delim"),
      headerDelimiter(GENERAL_DEFAULT_HEADER_DELIMITER),headerDelimiter_s("H"),headerDelimiter_l("head_delim"),
      pruneFeatures(GENERAL_DEFAULT_MIN_SAMPLES),pruneFeatures_s("X"),pruneFeatures_l("prune_features") {

      // Read the user parameters ...
      ArgParse parser(argc,argv);

      parser.getFlag(printHelp_s, printHelp_l, printHelp);
      parser.getArgument<string>(input_s, input_l, input);
      parser.getArgument<string>(targetStr_s, targetStr_l, targetStr);
      parser.getArgument<string>(output_s, output_l, output);
      parser.getArgument<string>(whiteList_s, whiteList_l, whiteList);
      parser.getArgument<string>(blackList_s, blackList_l, blackList);
      parser.getArgument<string>(log_s,log_l,log);
      string dataDelimiter,headerDelimiter;
      parser.getArgument<string>(dataDelimiter_s, dataDelimiter_l, dataDelimiter);
      parser.getArgument<string>(headerDelimiter_s, headerDelimiter_l, headerDelimiter);
      parser.getArgument<size_t>(pruneFeatures_s, pruneFeatures_l, pruneFeatures);
      stringstream ss(dataDelimiter);
      ss >> dataDelimiter;

      ss.clear();
      ss.str("");
      ss << headerDelimiter;
      ss >> headerDelimiter;
      
    }

    void help() {

      cout << "REQUIRED ARGUMENTS:" << endl;
      cout << " -" << input_s << " / --" << input_l << setw( maxWidth - input_l.size() )
           << " " << "Input data file, either AFM or ARFF" << endl;
      cout << " -" << targetStr_s << " / --" << targetStr_l << setw( maxWidth - targetStr_l.size() )
           << " " << "Target, specified as integer or string that is to be matched with the content of input" << endl;
      cout << " -" << output_s << " / --" << output_l << setw( maxWidth - output_l.size() )
           << " " << "Output file" << endl;
      cout << endl;
      
      cout << "OPTIONAL ARGUMENTS:" << endl;
      cout << " -" << log_s << " / --" << log_l << setw( maxWidth - log_l.size() )
           << " " << "Log output file" << endl;
      cout << " -" << whiteList_s << " / --" << whiteList_l << setw( maxWidth - whiteList_l.size() )
           << " " << "White list of features to be included in the input file(s)." << endl;
      cout << " -" << blackList_s << " / --" << blackList_l << setw( maxWidth - blackList_l.size() )
           << " " << "Black list of features to be excluded from the input file(s)." << endl;
      cout << " -" << dataDelimiter_s << " / --" << dataDelimiter_l << setw( maxWidth - dataDelimiter_l.size() )
           << " " << "Data delimiter (default \\t)" << endl;
      cout << " -" << headerDelimiter_s << " / --" << headerDelimiter_l << setw( maxWidth - headerDelimiter_l.size() )
           << " " << "Header delimiter that separates the N and C symbols in feature headers from the rest (default " << GENERAL_DEFAULT_HEADER_DELIMITER << ")" << endl;
      cout << " -" << pruneFeatures_s << " / --" << pruneFeatures_l << setw( maxWidth - pruneFeatures_l.size() )
           << " " << "Features with less than n ( default " << GENERAL_DEFAULT_MIN_SAMPLES << " ) samples will be removed" << endl;
      cout << endl;

    }
    
  };
    
  struct RF_options {
    
    size_t nTrees; const string nTrees_s; const string nTrees_l;
    size_t mTry; const string mTry_s; const string mTry_l;
    size_t nMaxLeaves; const string nMaxLeaves_s; const string nMaxLeaves_l;
    size_t nodeSize; const string nodeSize_s; const string nodeSize_l;
    size_t nPerms; const string nPerms_s; const string nPerms_l;
    num_t pValueThreshold; const string pValueThreshold_s; const string pValueThreshold_l;
    
    RF_options(const int argc, char* const argv[]):
      
      nTrees(RF_DEFAULT_N_TREES),nTrees_s("n"),nTrees_l("RF_ntrees"),
      mTry(RF_DEFAULT_M_TRY),mTry_s("m"),mTry_l("RF_mtry"),
      nMaxLeaves(RF_DEFAULT_N_MAX_LEAVES),nMaxLeaves_s("a"),nMaxLeaves_l("RF_maxleaves"),
      nodeSize(RF_DEFAULT_NODE_SIZE),nodeSize_s("s"),nodeSize_l("RF_nodesize"),
      nPerms(RF_DEFAULT_N_PERMS),nPerms_s("p"),nPerms_l("RF_nperms"),
      pValueThreshold(RF_DEFAULT_P_VALUE_THRESHOLD),pValueThreshold_s("t"),pValueThreshold_l("pthreshold") {
      
      // Read the user parameters ...
      ArgParse parser(argc,argv);
            
      parser.getArgument<size_t>(nTrees_s,nTrees_l,nTrees);
      parser.getArgument<size_t>(mTry_s, mTry_l, mTry);
      parser.getArgument<size_t>(nMaxLeaves_s, nMaxLeaves_l, nMaxLeaves);
      parser.getArgument<size_t>(nodeSize_s, nodeSize_l, nodeSize);
      parser.getArgument<size_t>(nPerms_s, nPerms_l, nPerms);
      parser.getArgument(pValueThreshold_s, pValueThreshold_l, pValueThreshold);
      
    }

    void overview() {
      cout << "PROGRAM: RF-ACE-FILTER" << endl << endl;
      cout << "Given target feature, applies decision tree ensemble learning with RF-ACE " << endl;
      cout << "to identify statistically significant predictors." << endl;
    }
    
    void help() {
      
      cout << "OPTIONAL ARGUMENTS -- RANDOM FOREST:" << endl;
      cout << " -" << nTrees_s << " / --" << nTrees_l << setw( maxWidth - nTrees_l.size() )
	   << " " << "Number of trees per RF (default " << RF_DEFAULT_N_TREES << ")" << endl;
      cout << " -" << mTry_s << " / --" << mTry_l << setw( maxWidth - mTry_l.size() )
	   << " " << "Number of randomly drawn features per node split (default floor(0.1*nFeatures))" << endl;
      cout << " -" << nMaxLeaves_s << " / --" << nMaxLeaves_l << setw( maxWidth - nMaxLeaves_l.size() )
	   << " " << "Maximum number of leaves per tree (default " << RF_DEFAULT_N_MAX_LEAVES << ")" << endl;
      cout << " -" << nodeSize_s << " / --" << nodeSize_l << setw( maxWidth - nodeSize_l.size() )
	   << " " << "Minimum number of train samples per node, affects tree depth (default " << RF_DEFAULT_NODE_SIZE << ")" << endl;
      cout << " -" << nPerms_s << " / --" << nPerms_l << setw( maxWidth - nPerms_l.size() )
	   << " " << "Number of Random Forests (default " << RF_DEFAULT_N_PERMS << ")" << endl;
      cout << " -" << pValueThreshold_s << " / --" << pValueThreshold_l << setw( maxWidth - pValueThreshold_l.size() )
	   << " " << "p-value threshold below which associations are listed (default "
	   << RF_DEFAULT_P_VALUE_THRESHOLD << ")" << endl;
      cout << endl;

    }
    
  };
  
  struct GBT_options {
    
          size_t nTrees;
    const string nTrees_s;
    const string nTrees_l;
    
          size_t nMaxLeaves;
    const string nMaxLeaves_s;
    const string nMaxLeaves_l;
    
           num_t shrinkage;
    const string shrinkage_s;
    const string shrinkage_l;
    
           num_t subSampleSize;
    const string subSampleSize_s;
    const string subSampleSize_l;
    
    GBT_options(const int argc, char* const argv[]):    
      nTrees(GBT_DEFAULT_N_TREES),
      nTrees_s("r"),
      nTrees_l("GBT_ntrees"),
      
      nMaxLeaves(GBT_DEFAULT_N_MAX_LEAVES),
      nMaxLeaves_s("l"),
      nMaxLeaves_l("GBT_maxleaves"),
      
      shrinkage(GBT_DEFAULT_SHRINKAGE),
      shrinkage_s("z"),
      shrinkage_l("GBT_shrinkage"),
      
      subSampleSize(GBT_DEFAULT_SUB_SAMPLE_SIZE),
      subSampleSize_s("u"),
      subSampleSize_l("GBT_samplesize") {

      // Read the user parameters ...
      ArgParse parser(argc,argv);      
      
      parser.getArgument<size_t>(nTrees_s, nTrees_l, nTrees);
      parser.getArgument<size_t>(nMaxLeaves_s, nMaxLeaves_l, nMaxLeaves);
      parser.getArgument<num_t>(shrinkage_s, shrinkage_l, shrinkage);
      parser.getArgument<num_t>(subSampleSize_s, subSampleSize_l, subSampleSize);
      
    }

    void help() {
      
      cout << "OPTIONAL ARGUMENTS -- GRADIENT BOOSTING TREES:" << endl;
      cout << " -" << nTrees_s << " / --" << nTrees_l << setw( maxWidth - nTrees_l.size() )
	   << " " << "Number of trees in the GBT (default " << GBT_DEFAULT_N_TREES << ")" << endl;
      cout << " -" << nMaxLeaves_s << " / --" << nMaxLeaves_l << setw( maxWidth - nMaxLeaves_l.size() )
	   << " " << "Maximum number of leaves per tree (default " << GBT_DEFAULT_N_MAX_LEAVES << ")" << endl;
      cout << " -" << shrinkage_s << " / --" << shrinkage_l << setw( maxWidth - shrinkage_l.size() )
	   << " " << "Shrinkage applied to evolving the residual (default " << GBT_DEFAULT_SHRINKAGE << ")" << endl;
      cout << " -" << subSampleSize_s << " / --" << subSampleSize_l << setw( maxWidth - subSampleSize_l.size() )
	   << " " << "Sample size fraction for training the trees (default " << GBT_DEFAULT_SUB_SAMPLE_SIZE << ")" << endl;
      cout << endl;


    }

  };

  void validateOptions(const General_options& gen_op) {
    
    // Print help and exit if input file is not specified
    if ( gen_op.input == "" ) {
      cerr << "Input file not specified" << endl;
      printHelpHint();
      exit(1);
    }
    
    // Print help and exit if target index is not specified
    if ( gen_op.targetStr == "" ) {
      cerr << "target(s) (-i/--target) not specified" << endl;
      printHelpHint();
      exit(1);
    }
    
    if ( gen_op.output == "" ) {
      cerr << "You forgot to specify an output file!" << endl;
      printHelpHint();
      exit(1);
    }

  }
  
}

#endif
