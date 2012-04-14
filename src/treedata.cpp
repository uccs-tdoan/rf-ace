#include "treedata.hpp"
#include <cstdlib>
#include <fstream>
#include <cassert>
#include <iostream>
#include <sstream>
#include <utility>
#include <algorithm>
#include <ctime>

#include "math.hpp"
#include "utils.hpp"

using namespace std;

/**
   Reads a data file into a Treedata object. The data file can be either AFM or ARFF
   NOTE: dataDelimiter and headerDelimiter are used only when the format is AFM, for 
   ARFF default delimiter (comma) is used 
*/
Treedata::Treedata(string fileName, char dataDelimiter, char headerDelimiter, int seed):
  dataDelimiter_(dataDelimiter),
  headerDelimiter_(headerDelimiter),
  features_(0),
  sampleHeaders_(0) {

  //Initialize stream to read from file
  ifstream featurestream;
  featurestream.open(fileName.c_str());
  if ( !featurestream.good() ) {
    cerr << "Failed to open file '" << fileName << "' for reading. Make sure the file exists. Quitting..." << endl;
    exit(1);
  }

  // Interprets file type from the content of the file
  FileType fileType = UNKNOWN;
  Treedata::readFileType(fileName,fileType);

  // Reads raw data matrix from the input file
  // NOTE: should be optimized to scale for large data sets
  vector<vector<string> > rawMatrix;
  vector<string> featureHeaders;
  vector<bool> isFeatureNumerical;
  if(fileType == AFM) {
    
    // Reads from the AFM stream and inserts 
    // data to the following arguments
    Treedata::readAFM(featurestream,
		      rawMatrix,
		      featureHeaders,
		      sampleHeaders_,
		      isFeatureNumerical);
    
  } else if(fileType == ARFF) {
    
    // Reads from the ARFF stream and inserts 
    // data to the following arguments
    Treedata::readARFF(featurestream,
		       rawMatrix,
		       featureHeaders,
		       isFeatureNumerical);

    // ARFF doesn't contain sample headers 
    sampleHeaders_.clear();
    sampleHeaders_.resize(rawMatrix[0].size(),
			  "NO_SAMPLE_ID");
    
  } else {
    
    // By default, reads from the AFM stream and inserts
    // data to the following arguments
    Treedata::readAFM(featurestream,
		      rawMatrix,
		      featureHeaders,
		      sampleHeaders_,
		      isFeatureNumerical);
    
  }      

  // Extract the number of features 
  size_t nFeatures = featureHeaders.size();

  // Resize the feature data container to fit the 
  // original AND contrast features ( so 2*nFeatures )
  features_.resize(2*nFeatures);

  // Start reading data to the final container "features_"
  for(size_t i = 0; i < nFeatures; ++i) {
    
    // We require that no two features have identical header
    if( name2idx_.find(featureHeaders[i]) != name2idx_.end() ) {
      cerr << "Duplicate feature header '" << featureHeaders[i] << "' found!" << endl;
      exit(1);
    }

    // Map the i'th feature header to integer i
    // NOTE: could be replaced with a hash table
    name2idx_[featureHeaders[i]] = i;

    // Get name for the i'th feature
    features_[i].name = featureHeaders[i];

    // Get type for the i'th feature
    features_[i].isNumerical = isFeatureNumerical[i];

    if(features_[i].isNumerical) {

      // If type is numerical, read the raw data as numbers
      datadefs::strv2numv(rawMatrix[i],
			  features_[i].data);

    } else {

      // If type is categorical, read the raw data as string literals
      // NOTE: mapping and backMapping store the information how to translate
      //       the raw data (string literals) to the internal format (numbers) 
      //       and back
      datadefs::strv2catv(rawMatrix[i], 
			  features_[i].data, 
			  features_[i].mapping, 
			  features_[i].backMapping);

    }

  } 
 
  // Generate contrast features
  for(size_t i = nFeatures; i < 2*nFeatures; ++i) {
    features_[i] = features_[ i - nFeatures ];
    string contrastName = features_[ i - nFeatures ].name;
    features_[i].name = contrastName.append("_CONTRAST");
    name2idx_[contrastName] = i;
  }

  // Initialize the Mersenne Twister random number generator
  if ( seed < 0 ) {
    seed = utils::generateSeed();
  }
  randomInteger_.seed( static_cast<unsigned int>(seed) );

  // Permute contrasts, so that the data becomes just noise
  this->permuteContrasts();

}

Treedata::~Treedata() {
  /* Empty destructor */
}

void Treedata::whiteList(const set<string>& featureNames ) {
  
  vector<bool> keepFeatureIcs(this->nFeatures(),false);

  for ( set<string>::const_iterator it( featureNames.begin() ); it != featureNames.end(); ++it ) {
    keepFeatureIcs[this->getFeatureIdx(*it)] = true;
  }

  this->whiteList(keepFeatureIcs);

}

void Treedata::blackList(const set<string>& featureNames ) {
  
  vector<bool> keepFeatureIcs(this->nFeatures(),true);
  
  for ( set<string>::const_iterator it( featureNames.begin() ); it != featureNames.end(); ++it ) {
    keepFeatureIcs[this->getFeatureIdx(*it)] = false;
  }
  
  this->whiteList(keepFeatureIcs);

}

void Treedata::whiteList(const vector<bool>& keepFeatureIcs) {
  
  assert( keepFeatureIcs.size() == this->nFeatures() );
  
  size_t nFeaturesNew = 0;
  for ( size_t i = 0; i < keepFeatureIcs.size(); ++i ) {
    if ( keepFeatureIcs[i] ) {
      ++nFeaturesNew;
    }
  }

  vector<Feature> featureCopy = features_;
  features_.resize(2*nFeaturesNew);

  map<string,size_t> name2idxCopy = name2idx_;

  name2idx_.clear();

  size_t nFeatures = keepFeatureIcs.size();
  size_t iter = 0;
  for ( size_t i = 0; i < nFeatures; ++i ) {

    if ( !keepFeatureIcs[i] ) {
      continue;
    }
    
    string featureName = featureCopy[i].name;

    if ( name2idxCopy.find(featureName) == name2idxCopy.end() ) {
      cerr << "Treedata::keepFeatures() -- feature '" << featureName << "' does not exist" << endl;
      exit(1);
    }

    features_[ iter ] = featureCopy[ name2idxCopy[featureName] ];
    name2idx_[ featureName ] = iter;

    featureName.append("_CONTRAST");

    if ( name2idxCopy.find(featureName) == name2idxCopy.end() ) {
      cerr << "Treedata::keepFeatures() -- feature '" << featureName << "' does not exist" << endl;
      exit(1);
    }

    features_[ nFeaturesNew + iter ] = featureCopy[ name2idxCopy[featureName] ];
    name2idx_[ featureName ] = nFeaturesNew + iter; 

    ++iter;

  }

  assert( iter == nFeaturesNew );
  assert( 2*iter == features_.size() );
  
}

void Treedata::readFileType(string& fileName, FileType& fileType) {

  stringstream ss(fileName);
  string suffix = "";
  while(getline(ss,suffix,'.')) {}
  //datadefs::toupper(suffix);

  if(suffix == "AFM" || suffix == "afm") {
    fileType = AFM;
  } else if(suffix == "ARFF" || suffix == "arff") {
    fileType = ARFF;
  } else {
    fileType = UNKNOWN;
  }

}

void Treedata::readAFM(ifstream& featurestream, 
		       vector<vector<string> >& rawMatrix, 
		       vector<string>& featureHeaders, 
		       vector<string>& sampleHeaders,
		       vector<bool>& isFeatureNumerical) {

  string field;
  string row;

  rawMatrix.clear();
  featureHeaders.clear();
  sampleHeaders.clear();
  isFeatureNumerical.clear();

  //Remove upper left element from the matrix as useless
  getline(featurestream,field,dataDelimiter_);

  //Next read the first row, which should contain the column headers
  getline(featurestream,row);
  stringstream ss( utils::chomp(row) );
  bool isFeaturesAsRows = true;
  vector<string> columnHeaders;
  while ( getline(ss,field,dataDelimiter_) ) {

    // If at least one of the column headers is a valid feature header, we assume features are stored as columns
    if ( isFeaturesAsRows && isValidFeatureHeader(field) ) {
      isFeaturesAsRows = false;
    }
    columnHeaders.push_back(field);
  }

  // We should have reached the end of file. NOTE: failbit is set since the last element read did not end at '\t'
  assert( ss.eof() );
  //assert( !ss.fail() );

  size_t nColumns = columnHeaders.size();

  vector<string> rowHeaders;
  //vector<string> sampleHeaders; // THIS WILL BE DEFINED AS ONE OF THE INPUT ARGUMENTS

  //Go through the rest of the rows
  while ( getline(featurestream,row) ) {

    row = utils::chomp(row);

    //Read row from the stream
    ss.clear();
    ss.str("");

    //Read the string back to a stream
    ss << row;

    //Read the next row header from the stream
    getline(ss,field,dataDelimiter_);
    rowHeaders.push_back(field);

    vector<string> rawVector(nColumns);
    for(size_t i = 0; i < nColumns; ++i) {
      getline(ss,rawVector[i],dataDelimiter_);
    }
    assert(!ss.fail());
    assert(ss.eof());
    rawMatrix.push_back(rawVector);
  }

  //If the data is row-formatted...
  if(isFeaturesAsRows) {
    //cout << "AFM orientation: features as rows" << endl;

    //... and feature headers are row headers
    featureHeaders = rowHeaders;
    sampleHeaders = columnHeaders;

  } else {

    //cout << "AFM orientation: features as columns" << endl;
      
    Treedata::transpose<string>(rawMatrix);
      
    //... and feature headers are row headers
    featureHeaders = columnHeaders;
    sampleHeaders = rowHeaders;
      
  }

  size_t nFeatures = featureHeaders.size();
  isFeatureNumerical.resize(nFeatures);
  for(size_t i = 0; i < nFeatures; ++i) {
    if(Treedata::isValidNumericalHeader(featureHeaders[i])) {
      isFeatureNumerical[i] = true;
    } else {
      isFeatureNumerical[i] = false;
    }
  }
}

void Treedata::readARFF(ifstream& featurestream, vector<vector<string> >& rawMatrix, vector<string>& featureHeaders, vector<bool>& isFeatureNumerical) {

  string row;

  bool hasRelation = false;
  bool hasData = false;

  size_t nFeatures = 0;
  //TODO: add Treedata::clearData(...)
  rawMatrix.clear();
  featureHeaders.clear();
  isFeatureNumerical.clear();
  
  //Read one line from the ARFF file
  while ( getline(featurestream,row) ) {

    row = utils::chomp(row);

    //Comment lines and empty lines are omitted
    if(row[0] == '%' || row == "") {
      continue;
    }
    
    string rowU = datadefs::toUpperCase(row);
    
    //Read relation
    if(!hasRelation && rowU.compare(0,9,"@RELATION") == 0) {
      hasRelation = true;
      //cout << "found relation header: " << row << endl;
    } else if ( rowU.compare(0,10,"@ATTRIBUTE") == 0) {    //Read attribute
      string attributeName = "";
      bool isNumerical;
      ++nFeatures;
      //cout << "found attribute header: " << row << endl;
      Treedata::parseARFFattribute(row,attributeName,isNumerical);
      featureHeaders.push_back(attributeName);
      isFeatureNumerical.push_back(isNumerical);
      
    } else if(!hasData && rowU.compare(0,5,"@DATA") == 0) {    //Read data header
      
      hasData = true;
      break;
      //cout << "found data header:" << row << endl;
    } else {      //If none of the earlier branches matched, we have a problem
      cerr << "incorrectly formatted ARFF row '" << row << "'" << endl;
      assert(false);
    }
    
  }

  if ( !hasData ) {
    cerr << "Treedata::readARFF() -- could not find @data/@DATA identifier" << endl;
    exit(1);
  }

  if ( !hasRelation ) {
    cerr << "Treedata::readARFF() -- could not find @relation/@RELATION identifier" << endl;
    exit(1);
  }
    
  //Read data row-by-row
  while ( getline(featurestream,row) ) {
    
    row = utils::chomp(row);

    //Comment lines and empty lines are omitted
    if ( row == "" ) {
      continue;
    }
    
    // One sample is stored as row in the matrix
    rawMatrix.push_back( utils::split(row,',') );
    
    if ( rawMatrix.back().size() != nFeatures ) {
      cerr << "Treedata::readARFF() -- sample contains incorrect number of features" << endl;
      exit(1);
    }
    
  }
  
  this->transpose<string>(rawMatrix);
  
}

void Treedata::parseARFFattribute(const string& str, string& attributeName, bool& isFeatureNumerical) {

  stringstream ss(str);
  string attributeHeader = "";
  attributeName = "";
  string attributeType = "";

  getline(ss,attributeHeader,' ');
  getline(ss,attributeName,' ');
  getline(ss,attributeType);

  //string prefix;
  if(datadefs::toUpperCase(attributeType) == "NUMERIC" ||
     datadefs::toUpperCase(attributeType) == "REAL" ) {
    isFeatureNumerical = true;
  } else {
    isFeatureNumerical = false;
  }
  //prefix.append(attributeName);
  //attributeName = prefix;
}

bool Treedata::isValidNumericalHeader(const string& str) {
  
  stringstream ss(str);
  string typeStr;
  getline(ss,typeStr,headerDelimiter_);

  return(  typeStr == "N" );
}

bool Treedata::isValidCategoricalHeader(const string& str) {

  stringstream ss(str);
  string typeStr;
  getline(ss,typeStr,headerDelimiter_);

  return( typeStr == "C" || typeStr == "B" );
}

bool Treedata::isValidFeatureHeader(const string& str) {
  return( isValidNumericalHeader(str) || isValidCategoricalHeader(str) );
}

size_t Treedata::nFeatures() {
  return( features_.size() / 2 );
}

size_t Treedata::nSamples() {
  return( sampleHeaders_.size() );
}

// WILL BECOME DEPRECATED
num_t Treedata::pearsonCorrelation(size_t featureIdx1, size_t featureIdx2) {
  
  vector<num_t> featureData1,featureData2;

  vector<size_t> sampleIcs = utils::range( this->nSamples() );

  this->getFilteredFeatureDataPair(featureIdx1,featureIdx2,sampleIcs,featureData1,featureData2);

  return( math::pearsonCorrelation(featureData1,featureData2) );

}

size_t Treedata::getFeatureIdx(const string& featureName) {
  if ( name2idx_.find(featureName) == name2idx_.end() ) {
    cerr << "Treedata::getFeatureIdx() -- feature '" << featureName << "' does not exist" << endl;
    exit(1);
  }
  return( name2idx_[featureName] );
}

string Treedata::getFeatureName(const size_t featureIdx) {
  return(features_[featureIdx].name);
}

string Treedata::getSampleName(const size_t sampleIdx) {
  return(sampleHeaders_[sampleIdx]);
}


void Treedata::print() {
  cout << "Printing feature matrix (missing values encoded to " << datadefs::NUM_NAN << "):" << endl;
  for(size_t j = 0; j < Treedata::nSamples(); ++j) {
    cout << '\t' << "foo";
  }
  cout << endl;
  for(size_t i = 0; i < Treedata::nFeatures(); ++i) {
    cout << i << ':' << features_[i].name << ':';
    for(size_t j = 0; j < Treedata::nSamples(); ++j) {
      cout << '\t' << features_[i].data[j];
    }
    cout << endl;
  }
}


void Treedata::print(const size_t featureIdx) {
  cout << "Print " << features_[featureIdx].name << ":";
  for(size_t i = 0; i < Treedata::nSamples(); ++i) {
    cout << " " << features_[featureIdx].data[i];
  }
  cout << endl;
}


void Treedata::permuteContrasts() {

  size_t nFeatures = this->nFeatures();
  size_t nSamples = this->nSamples();

  for ( size_t i = nFeatures; i < 2*nFeatures; ++i ) {
    

    vector<size_t> sampleIcs = utils::range( nSamples );

    vector<num_t> filteredData = this->getFilteredFeatureData(i,sampleIcs);
    
    this->permute<num_t>(filteredData);

    //datadefs::print(features_[i].data);

    for ( size_t j = 0; j < sampleIcs.size(); ++j ) {
      features_[i].data[ sampleIcs[j] ] = filteredData[j];
    }

    //datadefs::print(features_[i].data);
  }

}

bool Treedata::isFeatureNumerical(const size_t featureIdx) {
  return(features_[featureIdx].isNumerical);
}

size_t Treedata::nRealSamples(const size_t featureIdx) { 
  
  size_t nRealSamples;
  datadefs::countRealValues( features_[featureIdx].data, nRealSamples );
  return( nRealSamples );

}

size_t Treedata::nRealSamples(const size_t featureIdx1, const size_t featureIdx2) {

  size_t nRealSamples = 0;
  for( size_t i = 0; i < Treedata::nSamples(); ++i ) {
    if( !datadefs::isNAN( features_[featureIdx1].data[i] ) && !datadefs::isNAN( features_[featureIdx2].data[i] ) ) {
      ++nRealSamples;
    }
  }
  return( nRealSamples );
}

size_t Treedata::nCategories(const size_t featureIdx) {
  return( features_[featureIdx].mapping.size() );
}

size_t Treedata::nMaxCategories() {

  size_t ret = 0;
  for( size_t i = 0; i < Treedata::nFeatures(); ++i ) {
    if( ret < features_[i].mapping.size() ) {
      ret = features_[i].mapping.size();
    }
  }
  
  return( ret ); 

}

vector<string> Treedata::categories(const size_t featureIdx) {
  
  vector<string> categories;

  if( this->isFeatureNumerical(featureIdx) ) {
    return( categories );
  }
 
  for ( map<num_t,string>::const_iterator it( features_[featureIdx].backMapping.begin() ) ; it != features_[featureIdx].backMapping.end(); ++it ) {
    categories.push_back(it->second);
  }

  return( categories );

}

template <typename T> void Treedata::transpose(vector<vector<T> >& mat) {

  vector<vector<T> > foo = mat;

  size_t ncols = mat.size();
  size_t nrows = mat[0].size();

  mat.resize(nrows);
  for(size_t i = 0; i < nrows; ++i) {
    mat[i].resize(ncols);
  }

  for(size_t i = 0; i < nrows; ++i) {
    for(size_t j = 0; j < ncols; ++j) {
      mat[i][j] = foo[j][i];
    }
  }
}

/*
  void Treedata::permute(vector<size_t>& ics) {
  for (size_t i = 0; i < ics.size(); ++i) {
  size_t j = randomInteger_() % (i + 1);
  ics[i] = ics[j];
  ics[j] = i;
  }
  }
  
  void Treedata::permute(vector<num_t>& data) {
  size_t n = data.size();
  vector<size_t> ics(n);
  
  Treedata::permute(ics);
  
  for(size_t i = 0; i < n; ++i) {
  num_t temp = data[i];
  data[i] = data[ics[i]];
  data[ics[i]] = temp;
  }
  }
*/


void Treedata::bootstrapFromRealSamples(const bool withReplacement, 
                                        const num_t sampleSize, 
                                        const size_t featureIdx, 
                                        vector<size_t>& ics, 
                                        vector<size_t>& oobIcs) {
    
  //Check that the sampling parameters are appropriate
  assert(sampleSize > 0.0);
  if(!withReplacement && sampleSize > 1.0) {
    cerr << "Treedata: when sampling without replacement, sample size must be less or equal to 100% (sampleSize <= 1.0)" << endl;
    exit(1);
  }

  //First we collect all indices that correspond to real samples
  vector<size_t> allIcs;
  for(size_t i = 0; i < Treedata::nSamples(); ++i) {
    if(!datadefs::isNAN(features_[featureIdx].data[i])) {
      allIcs.push_back(i);
    }
  }
  
  //Extract the number of real samples, and see how many samples do we have to collect
  size_t nRealSamples = allIcs.size();
  size_t nSamples = static_cast<size_t>( floor( sampleSize * nRealSamples ) );
  ics.resize(nSamples);
  
  //If sampled with replacement...
  if(withReplacement) {
    //Draw nSamples random integers from range of allIcs
    for(size_t sampleIdx = 0; sampleIdx < nSamples; ++sampleIdx) {
      ics[sampleIdx] = allIcs[randomInteger_() % nRealSamples];
    }
  } else {  //If sampled without replacement...
    vector<size_t> foo = utils::range(nRealSamples);
    this->permute<size_t>(foo);
    for(size_t i = 0; i < nSamples; ++i) {
      ics[i] = allIcs[foo[i]];
    }
  }

  sort(ics.begin(),ics.end());

  if(nSamples < nRealSamples) {
    oobIcs.resize(nRealSamples);
  } else {
    oobIcs.resize(nSamples);
  }

  //Then, as we now have the sample stored in ics, we'll check which of the samples, from allIcs, are not contained in ics and store them in oobIcs instead
  vector<size_t>::iterator it = set_difference(allIcs.begin(),allIcs.end(),ics.begin(),ics.end(),oobIcs.begin());
  size_t nOob = distance(oobIcs.begin(),it);
  oobIcs.resize(nOob);
  //cout << "nOob=" << nOob << endl;
}


vector<num_t> Treedata::getFeatureData(size_t featureIdx) {
  
  vector<num_t> data( features_[featureIdx].data.size() );

  for(size_t i = 0; i < Treedata::nSamples(); ++i) {
    data[i] = features_[featureIdx].data[i];
  }

  return( data );
}


num_t Treedata::getFeatureData(size_t featureIdx, const size_t sampleIdx) {

  num_t data = features_[featureIdx].data[sampleIdx];

  return( data ); 
}

vector<num_t> Treedata::getFeatureData(size_t featureIdx, const vector<size_t>& sampleIcs) {
  
  vector<num_t> data(sampleIcs.size());
  
  for(size_t i = 0; i < sampleIcs.size(); ++i) {
    data[i] = features_[featureIdx].data[sampleIcs[i]];
  }

  return( data );

}

vector<num_t> Treedata::getFilteredFeatureData(const size_t featureIdx,
					       vector<size_t>& sampleIcs) {

  size_t n = sampleIcs.size();

  vector<num_t> featureData(n);

  size_t nReal = 0;

  for ( size_t i = 0; i < n; ++i ) {
    size_t idx = sampleIcs[i];
    num_t value = features_[featureIdx].data[idx];
    if ( !datadefs::isNAN(value) ) {
      featureData[nReal] = value;
      sampleIcs[nReal] = idx;
      ++nReal;
    }
  }
  sampleIcs.resize(nReal);
  featureData.resize(nReal);

  return(featureData);

}


void Treedata::getFilteredFeatureDataPair(const size_t featureIdx1, 
					  const size_t featureIdx2, 
					  vector<size_t>& sampleIcs, 
					  vector<num_t>& featureData1, 
					  vector<num_t>& featureData2) {

  size_t n = sampleIcs.size();
  featureData1.resize(n);
  featureData2.resize(n);
  size_t nReal = 0;
  for(size_t i = 0; i < n; ++i) {

    num_t v1 = features_[featureIdx1].data[sampleIcs[i]];
    num_t v2 = features_[featureIdx2].data[sampleIcs[i]];
    
    if(!datadefs::isNAN(v1) && !datadefs::isNAN(v2)) {
      sampleIcs[nReal] = sampleIcs[i];
      featureData1[nReal] = v1;
      featureData2[nReal] = v2;
      ++nReal;
    }
  }
  featureData1.resize(nReal);
  featureData2.resize(nReal);
  sampleIcs.resize(nReal);

}

num_t Treedata::getCategoricalSplitFitness(const num_t sf_tot,
					   const num_t nsf_best,
					   const size_t n_tot) {

  return( -1.0 * sf_tot + 1.0 * n_tot * nsf_best ) / ( 1.0 * n_tot * n_tot - 1.0 * sf_tot );
  
}

num_t Treedata::getNumericalSplitFitness(const num_t se_tot,
					 const num_t se_best) {
  
  return( ( se_tot - se_best ) / se_tot );

}



// !! Correctness, Inadequate Abstraction: kill this method with fire. Refactor, REFACTOR, _*REFACTOR*_.
num_t Treedata::numericalFeatureSplit(const size_t targetIdx,
				      const size_t featureIdx,
				      const size_t minSamples,
				      vector<size_t>& sampleIcs_left,
				      vector<size_t>& sampleIcs_right,
				      num_t& splitValue) {

  num_t splitFitness = datadefs::NUM_NAN;

  vector<num_t> tv,fv;

  sampleIcs_left.clear();

  this->getFilteredAndSortedFeatureDataPair3(targetIdx,featureIdx,sampleIcs_right,tv,fv);

  size_t n_tot = fv.size();
  size_t n_right = n_tot;
  size_t n_left = 0;

  if(n_tot < 2 * minSamples) {
    return( datadefs::NUM_NAN );
  }

  int bestSplitIdx = -1;

  //If the target is numerical, we use the incremental squared error formula
  if ( this->isFeatureNumerical(targetIdx) ) {

    // We start with one sample on the left branch
    size_t n = 1;
    num_t mu_left = tv[0];
    vector<num_t> se_left(n_tot, 0.0);

    // Add one sample at a time, from right to left, while updating the
    // mean and squared error
    // NOTE1: leftmost sample has been added already, so we start i from 1
    // NOTE2: rightmost sample needs to be included since we want to compute
    //        squared error for the whole data vector ( i.e. se_tot )
    for( size_t i = 1; i < n_tot; ++i ) {

      // We need to manually transfer the previous squared error to the
      // present slot
      se_left[i] = se_left[i-1];

      // This function takes n'th data point tv[i] from right to left
      // and updates the mean and squared error
      math::incrementSquaredError(tv[i],++n,mu_left,se_left[i]);
    }

    // Make sure we accumulated the right number of samples
    assert( n == n_tot );

    // Make sure the squared error didn't become corrupted by NANs
    assert( !datadefs::isNAN(se_left.back()) );

    // Total squared error now equals to the squared error of left branch
    // wince all samples were taken to the left one by one
    num_t se_tot = se_left.back();

    // The best squared error is set to the worst case
    num_t se_best = se_left.back();

    // Now it's time to start adding samples from left to right
    // NOTE: it's intentional to set n to 0
    n = 0;
    num_t mu_right = 0.0;
    num_t se_right = 0.0;

    // Add samples one by one from left to right until we hit the
    // minimum allowed size of the branch
    for( int i = n_tot-1; i >= static_cast<int>(minSamples); --i ) {

      // Add n'th sample tv[i] from left to right and update
      // mean and squared error
      math::incrementSquaredError(tv[i],++n,mu_right,se_right);

      // If the sample is repeated and we can continue, continue
      if ( i-1 >= static_cast<int>(minSamples) && tv[i-1] == tv[i] ) {
        continue;
      }

      // If the split point "i-1" yields a better split than the previous one,
      // update se_best and bestSplitIdx
      if ( se_left[i-1] + se_right < se_best ) {

        bestSplitIdx = i-1;
        se_best = se_left[i-1] + se_right;

      }

    }

    // Make sure there were no NANs to corrupt the results
    assert( !datadefs::isNAN(se_right) );

    // Calculate split fitness
    splitFitness = this->getNumericalSplitFitness(se_tot,se_best);

  } else { // Otherwise we use the iterative gini index formula to update impurity scores while we traverse "right"

    // We start with one sample on the left branch
    size_t n_left = 1;
    map<num_t,size_t> freq_left;
    freq_left[ tv[0] ] = 1;
    vector<size_t> sf_left(n_tot, 0);
    sf_left[0] = 1;

    // Add one sample at a time, from right to left, while updating the
    // squared frequency
    // NOTE1: leftmost sample has been added already, so we start i from 1
    // NOTE2: rightmost sample needs to be included since we want to compute
    //        squared error for the whole data vector ( i.e. sf_tot )
    for( size_t i = 1; i < n_tot; ++i ) {

      // We need to manually transfer the previous squared frequency to the
      // present slot
      sf_left[i] = sf_left[i-1];

      // This function takes n'th data point tv[i] from right to left
      // and updates the squared frequency
      math::incrementSquaredFrequency(tv[i],freq_left,sf_left[i]);
      ++n_left;
    }

    // Make sure we accumulated the right number of samples
    assert( n_left == n_tot );

    // Total squared frequency now equals to the squared frequency of left branch
    // since all samples were taken to the left one by one
    size_t sf_tot = sf_left.back();

    // The best normalized squared frequency is set to the worst case
    num_t nsf_best = sf_left.back() / n_left;

    // Now it's time to start adding samples from right to left
    // NOTE: it's intentional to set n_right to 0
    size_t n_right = 0;
    map<num_t,size_t> freq_right;
    size_t sf_right = 0;

    // Add samples one by one from left to right until we hit the
    // minimum allowed size of the branch
    for( int i = n_tot-1; i >= static_cast<int>(minSamples); --i ) {

      // Add n'th sample tv[i] from right to left and update
      // mean and squared frequency
      math::incrementSquaredFrequency(tv[i],freq_right,sf_right);
      ++n_right;
      --n_left;

      // If we have repeated samples and can continue, continue
      if ( i-1 > static_cast<int>(minSamples) && tv[i-1] == tv[i] ) {
        continue;
      }

      // If the split point "i-1" yields a better split than the previous one,
      // update se_best and bestSplitIdx
      if(1.0*n_right*sf_left[i-1] + 1.0*n_left*sf_right > n_left*n_right*nsf_best && n_left >= minSamples) {
        bestSplitIdx = i-1;
        nsf_best = 1.0*sf_left[i-1] / n_left + 1.0*sf_right / n_right;
        //splitFitness = this->getSplitFitness(n_left,sf_left[i-1],n_right,sf_right,n_tot,sf_tot);
      }


    }

    splitFitness = this->getCategoricalSplitFitness(sf_tot,nsf_best,n_tot);
    // splitFitness = ( -1.0*sf_tot + 1.0 * n_tot * nsf_best ) / ( 1.0 * n_tot * n_tot - 1.0 * sf_tot );

  }


  if(bestSplitIdx == -1) {
    //cout << "N : " << n_left << " <-> " << n_right << " : fitness " << splitFitness << endl;
    return( datadefs::NUM_NAN );
  }

  splitValue = fv[bestSplitIdx];
  n_left = bestSplitIdx + 1;
  sampleIcs_left.resize(n_left);

  for(size_t i = 0; i < n_left; ++i) {
    sampleIcs_left[i] = sampleIcs_right[i];
  }
  sampleIcs_right.erase(sampleIcs_right.begin(),sampleIcs_right.begin() + n_left);
  n_right = sampleIcs_right.size();

  assert(n_left + n_right == n_tot);

  //cout << "N : " << n_left << " <-> " << n_right << " : fitness " << splitFitness << endl;

  return( splitFitness );
  
}

// !! Inadequate Abstraction: Refactor me.
num_t Treedata::categoricalFeatureSplit(const size_t targetIdx,
					const size_t featureIdx,
					const size_t minSamples,
					vector<size_t>& sampleIcs_left,
					vector<size_t>& sampleIcs_right,
					set<num_t>& splitValues_left,
					set<num_t>& splitValues_right) {

  num_t splitFitness = datadefs::NUM_NAN;

  vector<num_t> tv,fv;

  //cout << " -- sampleIcs_right.size() = " << sampleIcs_right.size();

  sampleIcs_left.clear();
  this->getFilteredFeatureDataPair(targetIdx,featureIdx,sampleIcs_right,tv,fv);

  //cout << " => " << sampleIcs_right.size() << endl;

  // Map all feature categories to the corresponding samples and represent it as map. The map is used to assign samples to left and right branches
  map<num_t,vector<size_t> > fmap_right;
  map<num_t,vector<size_t> > fmap_left;

  size_t n_tot = 0;
  datadefs::map_data(fv,fmap_right,n_tot);
  size_t n_right = n_tot;
  size_t n_left = 0;

  if(n_tot < 2 * minSamples) {
    return( datadefs::NUM_NAN );
  }

  if ( this->isFeatureNumerical(targetIdx) ) {

    num_t mu_right = math::mean(tv);
    num_t se_right = math::squaredError(tv,mu_right);

    num_t mu_left = 0.0;
    num_t se_left = 0.0;

    assert(n_tot == n_right);

    num_t se_best = se_right;
    num_t se_tot = se_right;

    while ( fmap_right.size() > 1 ) {

      map<num_t,vector<size_t> >::iterator it_best( fmap_right.end() );

      // We test each category one by one and see if the fitness becomes improved
      for ( map<num_t,vector<size_t> >::iterator it( fmap_right.begin() ); it != fmap_right.end() ; ++it ) {

        //cout << "Testing to split with feature '" << treedata->getRawFeatureData(featureIdx,it->first) << "'" << endl;

        // Take samples from right and put them left
        //cout << "from right to left: [";
        for(size_t i = 0; i < it->second.size(); ++i) {
          //cout << " " << it->second[i];

          // Add sample to left
          ++n_left;
	  math::incrementSquaredError(tv[ it->second[i] ], n_left, mu_left, se_left);

          // Remove sample from right
          --n_right;
	  math::decrementSquaredError(tv[ it->second[i] ], n_right, mu_right, se_right);

        }
        //cout << " ]" << endl;

        //If the split reduces impurity even further, save the point
        if ( se_left + se_right < se_best ) { //&& n_left >= minSamples && n_right >= minSamples )

          //cout << " => BETTER" << endl;
          it_best = it;
          se_best = se_left + se_right;
        }

        //Take samples from left and put them right
        //cout << "From left to right: [";
        for(size_t i = 0; i < it->second.size(); ++i) {
          //cout << " " << it->second[i];

          // Add sample to right
          ++n_right;
	  math::incrementSquaredError(tv[ it->second[i] ], n_right, mu_right, se_right);

          // Remove sample from left
          --n_left;
	  math::decrementSquaredError(tv[ it->second[i] ], n_left, mu_left, se_left);

        }
        //cout << " ]" << endl;

      }

      // After testing all categories,
      // if we couldn't find any split that would reduce impurity,
      // we'll exit the loop
      if ( it_best == fmap_right.end() ) {
        //cout << " -- STOP --" << endl;
        break;
      }

      // Otherwise move samples from right to left
      for(size_t i = 0; i < it_best->second.size(); ++i) {
        //cout << " " << it->second[i];

        // Add sample to left
        ++n_left;
	math::incrementSquaredError(tv[ it_best->second[i] ], n_left, mu_left, se_left);

        // Remove sample from right
        --n_right;
	math::decrementSquaredError(tv[ it_best->second[i] ], n_right, mu_right, se_right);

      }

      // Update the maps
      fmap_left.insert( *it_best );
      fmap_right.erase( it_best->first );

    }

    // Calculate the final split fitness
    splitFitness = this->getNumericalSplitFitness(se_tot,se_best);

  } else {

    map<num_t,size_t> freq_left,freq_right;
    size_t sf_left = 0;
    size_t sf_right = 0;

    for( size_t i = 0; i < n_tot; ++i ) {
      math::incrementSquaredFrequency(tv[i], freq_right, sf_right);
    }

    //datadefs::sqfreq(tv,freq_right,sf_right,n_right);
    assert(n_tot == n_right);

    size_t sf_tot = sf_right;
    num_t nsf_best = 1.0 * sf_right / n_right;

    while ( fmap_right.size() > 1 ) {

      map<num_t,vector<size_t> >::iterator it_best( fmap_right.end() );
      //cout << "There are " << fmap_right.size() << " categories on right" << endl;

      // We test each category one by one and see if the fitness becomes improved
      for ( map<num_t,vector<size_t> >::iterator it( fmap_right.begin() ); it != fmap_right.end() ; ++it ) {

        //cout << "Testing to split with feature '" << treedata->getRawFeatureData(featureIdx,it->first) << "'" << endl;

        // Take samples from right and put them left
        //cout << "from right to left: [";
        for(size_t i = 0; i < it->second.size(); ++i) {

          // Add sample to left
          ++n_left;
	  math::incrementSquaredFrequency(tv[ it->second[i] ], freq_left, sf_left);

          // Remove sample from right
          --n_right;
	  math::decrementSquaredFrequency(tv[ it->second[i] ], freq_right, sf_right);

        }
        //cout << " ]" << endl;

        //If the impurity becomes reduced even further, save the point
        if ( 1.0*n_right*sf_left + 1.0*n_left*sf_right > 1.0*n_left*n_right*nsf_best ) { //&& n_left >= minSamples && n_right >= minSamples )

          nsf_best = 1.0*sf_left/n_left + 1.0*sf_right/n_right;
          it_best = it;
          //cout << "nsf_best is now " << nsf_best << endl;
        }

        // Take samples from left and put them right
        //cout << "From left to right: [";
        for(size_t i = 0; i < it->second.size(); ++i) {

          // Add sample to right
          ++n_right;
	  math::incrementSquaredFrequency(tv[ it->second[i] ], freq_right, sf_right);

          // Remove sample from left
          --n_left;
	  math::decrementSquaredFrequency(tv[ it->second[i] ], freq_left, sf_left);

        }
        //cout << " ]" << endl;

      }

      // After testing all categories,
      // if we couldn't find any split that would reduce impurity,
      // we'll exit the loop
      if ( it_best == fmap_right.end() ) {
        //cout << " -- STOP --" << endl;
        break;
      }

      // Take samples from right and put them left
      for(size_t i = 0; i < it_best->second.size(); ++i) {

        // Add sample to left
        ++n_left;
	math::incrementSquaredFrequency(tv[ it_best->second[i] ], freq_left, sf_left);

        // Remove sample from right
        --n_right;
	math::decrementSquaredFrequency(tv[ it_best->second[i] ], freq_right, sf_right);

      }

      // Update the maps
      fmap_left.insert( *it_best );
      fmap_right.erase( it_best->first );

    }

    // Calculate the final split fitness
    splitFitness = this->getCategoricalSplitFitness(sf_tot,nsf_best,n_tot);
    //splitFitness = ( -1.0*sf_tot + 1.0 * n_tot * nsf_best ) / ( 1.0 * n_tot * n_tot - 1.0 * sf_tot );
    // splitFitness = this->getSplitFitness(n_left,sf_left,n_right,sf_right,n_tot,sf_tot);

  }

  if( n_left < minSamples || n_right < minSamples ) {
    return( datadefs::NUM_NAN );
  }

  // Assign samples and categories on the left. First store the original sample indices
  vector<size_t> sampleIcs = sampleIcs_right;

  assert( n_left + n_right == n_tot );

  // Then populate the left side (sample indices and split values)
  sampleIcs_left.resize(n_left);
  splitValues_left.clear();
  size_t iter = 0;
  for ( map<num_t,vector<size_t> >::const_iterator it(fmap_left.begin()); it != fmap_left.end(); ++it ) {
    for ( size_t i = 0; i < it->second.size(); ++i ) {
      sampleIcs_left[iter] = sampleIcs[it->second[i]];
      ++iter;
    }
    splitValues_left.insert( it->first );
  }
  assert( iter == n_left);
  assert( splitValues_left.size() == fmap_left.size() );

  // Last populate the right side (sample indices and split values)
  sampleIcs_right.resize(n_right);
  splitValues_right.clear();
  iter = 0;
  for ( map<num_t,vector<size_t> >::const_iterator it(fmap_right.begin()); it != fmap_right.end(); ++it ) {
    for ( size_t i = 0; i < it->second.size(); ++i ) {
      sampleIcs_right[iter] = sampleIcs[it->second[i]];
      ++iter;
    }
    splitValues_right.insert( it->first );
  }

  assert( iter == n_right );
  assert( splitValues_right.size() == fmap_right.size() );

  return( splitFitness );

}


/*
  void Treedata::getFilteredAndSortedFeatureDataPair(const size_t targetIdx, 
  const size_t featureIdx, 
  vector<size_t>& sampleIcs, 
  vector<num_t>& targetData, 
  vector<num_t>& featureData) {
  
  if ( !features_[featureIdx].isNumerical ) {
  cerr << "Treedata::getFilteredAndSortedDataPair() -- cannot perform for CATEGORICAL features" << endl;
  exit(1);
  }
  
  //targetData.clear();
  //targetData.resize( sampleHeaders_.size(), datadefs::NUM_NAN );
  //featureData.clear();
  //featureData.resize( sampleHeaders_.size(), datadefs::NUM_NAN );
  
  //vector<size_t> sampleIcsCopy( sampleHeaders_.size() );
  //size_t maxPos = 0;
  
  // A map: sortOrderKey -> (sampleIdx,multiplicity)
  map<size_t,pair<size_t,size_t> > mapOrder;
  
  // Count the number of real samples
  size_t nReal = 0;
  
  // Go through all sample indices
  for ( vector<size_t>::const_iterator it(sampleIcs.begin()); it != sampleIcs.end(); ++it ) {
  
  // Extract the target and feature values for the index
  //num_t tVal = features_[targetIdx].data[*it];
  //num_t fVal = features_[featureIdx].data[*it];
  
  // If the data are non-NA...
  if ( !datadefs::isNAN(features_[featureIdx].data[*it]) && 
  !datadefs::isNAN(features_[targetIdx].data[*it]) ) {
  
  // Accumulate real data counter
  ++nReal;
  
  // Extract the ordered position of the sample
  size_t pos = features_[featureIdx].sortOrder[*it];
  
  // If the position is unused in the map...
  if ( mapOrder.find(pos) == mapOrder.end() ) {
  
  // Add the ordered position, the original sample index, 
  // and initialize the sample counter to 1
  pair<size_t,size_t> foo(*it,1);
  mapOrder.insert(pair<size_t,pair<size_t,size_t> >(pos,foo));
  } else {
  
  // Otherwise accumulate multiplicity by one
  ++mapOrder[pos].second;
  }
  }
  }
  
  targetData.resize(nReal);
  featureData.resize(nReal);
  sampleIcs.resize(nReal);
  
  size_t i = 0;
  
  for ( map<size_t,pair<size_t,size_t> >::const_iterator it(mapOrder.begin()); it != mapOrder.end(); ++it ) {
  
  for ( size_t j = 0; j < it->second.second; ++j ) {
  sampleIcs[i] = it->second.first;
  targetData[i] = features_[targetIdx].data[it->second.first];
  featureData[i] = features_[featureIdx].data[it->second.first];
  ++i;
  }
  }
  
  assert(i == nReal);
  
  }
*/


/*
  void Treedata::getFilteredAndSortedFeatureDataPair2(const size_t targetIdx,
  const size_t featureIdx,
  vector<size_t>& sampleIcs,
  vector<num_t>& targetData,
  vector<num_t>& featureData) {
  
  if ( !features_[featureIdx].isNumerical ) {
  cerr << "Treedata::getFilteredAndSortedDataPair() -- cannot perform for CATEGORICAL features" << endl;
  exit(1);
  }
  
  size_t n = sampleHeaders_.size();
  size_t s = sampleIcs.size();
  
  //vector<num_t> targetDataCopy(n);
  //vector<num_t> featureDataCopy(n);
  //vector<size_t> sampleIcsCopy(n);
  
  fill(temp_.multiplicity.begin(),temp_.multiplicity.end(),0);
  //vector<size_t> multiplicity(n, 0);
  
  //vector<size_t> sampleIcsCopy(  );
  size_t minPos = n;
  size_t maxPos = 0;
  
  // Count the number of real samples
  size_t nReal = 0;
  
  // Go through all sample indices
  for ( size_t i = 0; i < s; ++i ) {
  
  size_t ii = sampleIcs[i];
  
  // Extract the target and feature values for the index
  num_t tVal = features_[targetIdx].data[ii];
  num_t fVal = features_[featureIdx].data[ii];
  
  // If the data are non-NA...
  if ( !datadefs::isNAN(fVal) && 
  !datadefs::isNAN(tVal) ) {
  
  // Accumulate real data counter
  ++nReal;
  
  // Extract the ordered position of the sample
  size_t pos = features_[featureIdx].sortOrder[ii];
  ++temp_.multiplicity[pos];
  
  if ( temp_.multiplicity[pos] == 1 ) {
  temp_.featureDataCopy[pos] = fVal;
  temp_.targetDataCopy[pos] = tVal;
  temp_.sampleIcsCopy[pos] = ii;
  
  if ( pos > maxPos ) {
  maxPos = pos;
  }
  
  if ( pos < minPos ) {
  minPos = pos;
  }
  
  }
  
  }
  }
  
  featureData.resize(nReal);
  targetData.resize(nReal);
  sampleIcs.resize(nReal);
  
  size_t iter = 0;
  for ( size_t i = minPos; i <= maxPos; ++i ) {
  for ( size_t j = 0; j < temp_.multiplicity[i]; ++j ) {
  featureData[iter] = temp_.featureDataCopy[i];
  targetData[iter] = temp_.targetDataCopy[i];
  sampleIcs[iter] = temp_.sampleIcsCopy[i];
  ++iter;
  }
  }
  
  assert(nReal == iter);
  
  }
*/


void Treedata::getFilteredAndSortedFeatureDataPair3(const size_t targetIdx,
						    const size_t featureIdx,
						    vector<size_t>& sampleIcs,
						    vector<num_t>& targetData,
						    vector<num_t>& featureData) {


  featureData = this->getFeatureData(featureIdx,sampleIcs);
  //targetData = this->getFeatureData(targetIdx,sampleIcs);

  bool isIncreasingOrder = true;
  vector<size_t> refIcs;

  utils::filterSort(isIncreasingOrder,featureData,refIcs);
  //datadefs::sortFromRef<num_t>(targetData,refIcs);
  //datadefs::sortFromRef<size_t>(sampleIcs,refIcs);
  
  vector<size_t> sampleIcsCopy = sampleIcs;
  
  for ( size_t i = 0; i < refIcs.size(); ++i ) {
    sampleIcs[i] = sampleIcsCopy[refIcs[i]];
  }
  sampleIcs.resize(refIcs.size());
  
  targetData = this->getFeatureData(targetIdx,sampleIcs);
  

}

string Treedata::getRawFeatureData(const size_t featureIdx, const size_t sampleIdx) {

  num_t data = features_[featureIdx].data[sampleIdx];

  return( this->getRawFeatureData(featureIdx,data) );
    
}

string Treedata::getRawFeatureData(const size_t featureIdx, const num_t data) {

  // If the input data is NaN, we return NaN as string 
  if ( datadefs::isNAN(data) ) {
    return( datadefs::STR_NAN );
  }
  
  // If input feature is numerical, we just represent the numeric value as string
  if ( features_[featureIdx].isNumerical ) {
    stringstream ss;
    ss << data;
    return( ss.str() );
    return( utils::num2str(data) );
  } else {
    
    if ( features_[featureIdx].backMapping.find(data) == features_[featureIdx].backMapping.end() ) {
      cerr << "Treedata::getRawFeatureData() -- unknown value to get" << endl;
      exit(1);
    }
    
    return( features_[featureIdx].backMapping[data] );
  }
  
}

vector<string> Treedata::getRawFeatureData(const size_t featureIdx) {
  
  vector<string> rawData( sampleHeaders_.size() );

  for ( size_t i = 0; i < rawData.size(); ++i ) {
    rawData[i] = this->getRawFeatureData(featureIdx,i);
  }

  return( rawData );

}

void Treedata::replaceFeatureData(const size_t featureIdx, const vector<num_t>& featureData) {

  if(featureData.size() != features_[featureIdx].data.size() ) {
    cerr << "Treedata::replaceFeatureData(num_t) -- data dimension mismatch" << endl;
    exit(1);
  }

  // Since the data that was passed is numerical, we set isNumerical to true
  features_[featureIdx].isNumerical = true;

  // Data that is stored is directly the input data
  features_[featureIdx].data = featureData;

  // Update sort indices for fast lookup
  //this->updateSortOrder(featureIdx);

  // Since the data is not categorical, there's no need to provide mappings
  features_[featureIdx].mapping.clear();
  features_[featureIdx].backMapping.clear();

}

void Treedata::replaceFeatureData(const size_t featureIdx, const vector<string>& rawFeatureData) {

  if(rawFeatureData.size() != features_[featureIdx].data.size() ) {
    cerr << "Treedata::replaceFeatureData(string) -- data dimension mismatch" << endl;
    exit(1);
  }

  // Since the data that was passed are string literals, we set isNumerical to false
  features_[featureIdx].isNumerical = false;

  // Categorical data does not need sorting, thus, it doesn't benefit from the sort indices either
  //features_[featureIdx].sortOrder.clear();

  // The string literal data needs some processing 
  datadefs::strv2catv(rawFeatureData,
		      features_[featureIdx].data,
		      features_[featureIdx].mapping,
		      features_[featureIdx].backMapping);
}



