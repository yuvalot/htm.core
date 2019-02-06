
#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>

using namespace std;


int main() {
  cout << "ASCII" << endl;

  auto dataFilename = "./ascii.cpp";
  ifstream dataFile( dataFilename );
  string str((istreambuf_iterator<char>(dataFile)), istreambuf_iterator<char>());

  // Make model

  // Run model

  // Measure online stability

  // Measure inter/intra catagory overlap

  // Measure whole word classification accuracy
}
