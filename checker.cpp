// Compile instructions:
// compile: g++ checker.cpp -o checker -std=c++17 -g -pthread
// run: ./checker <submission files> -i=<comma seperated list of ends to ignore> -t=<instructor template files> -j=<threaded jobs>
// ex: ./checker Submissions -i=.mp4,.mk,.png,.jpg,.log,.xml,.o,.d,makefile -t=Template		// NOTE: no jobs

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <mutex>
#include <boost/algorithm/string.hpp>

#include "argh.h"
#include "dtl/dtl.hpp"


template<typename T> inline std::ostream& operator << (std::ostream& s, const std::vector<T>& a) { if(a.empty()) return s << "[]"; s << "["; for (const T& cur: a) s << cur << ", "; return s << "\b\b]"; }
template<typename T> inline std::ostream& operator << (std::ostream& s, const std::vector<T>&& a) { if(a.empty()) return s << "[]"; s << "["; for (const T& cur: a) s << cur << ", "; return s << "\b\b]"; }	

std::vector<std::string> split(std::string s, std::string delim){
	std::vector<std::string> out;
	if(s.empty()) return out;

	size_t start = 0, end = s.find(delim);
    while (end != std::string::npos) {
        out.push_back(s.substr(start, end - start));
        start = end + delim.length();
        end = s.find(delim, start);
    }

    out.push_back(s.substr(start, end));

	return out;
}

std::vector<std::filesystem::path> recursivelyFindAndFilterFiles(const std::vector<std::string>& rootPaths, const std::vector<std::string>& ignoredEnds){
	std::vector<std::filesystem::path> out;

	// For each of the root paths, recursively find all of the child files...
	for (auto dir: rootPaths)
		for (std::filesystem::recursive_directory_iterator i(dir), end; i != end; ++i) 
			if (!is_directory(i->path())){
				// Mark if the file should be kept (its end doesn't show up in the ignore list)
				bool keep = true;
				for(std::string ignore: ignoredEnds)
					if(boost::iends_with(i->path().relative_path().string(), ignore)){
						keep = false;
						break;
					}
				
				// If the file should be kept add it to the output array
				if(keep)
					out.emplace_back(absolute(i->path()));
			}

	return out;
}

std::vector<std::vector<size_t>> findCombinationIndecies(size_t N, size_t K) {
	std::vector<std::vector<size_t>> out;
    std::string bitmask(K, 1); // K leading 1's
    bitmask.resize(N, 0); // N-K trailing 0's
 
    // save indecies and permute bitmask
    do {
		std::vector<size_t> comb;
        for (size_t i = 0; i < N; ++i) // [0..N-1] integers
            if (bitmask[i]) comb.push_back(i);
        out.emplace_back(std::move(comb));
    } while (std::prev_permutation(bitmask.begin(), bitmask.end()));

	return out;
}

size_t findFirstSpace(const std::string& in, size_t start = 0){
	for(size_t i = start; i < in.size(); i++)
		if(std::isspace(in[i]) && in[i] != '\n')
			return i;

	return std::string::npos;
}

size_t findFirstNonSpace(const std::string& in, size_t start = 0){
	for(size_t i = start; i < in.size(); i++)
		if(!(std::isspace(in[i]) && in[i] != '\n'))
			return i;

	return std::string::npos;
}

std::string unifyWhitespace(std::string in){
	for(size_t start = findFirstSpace(in), end = findFirstNonSpace(in, start);
		start < in.size() && end < in.size(); start = findFirstSpace(in, start + 1),
		end = findFirstNonSpace(in, start))
			in.replace(start, end - start, " ");
	return in;
}

// Function which removes all of the deliminating characters from the left side of a string
static std::string ltrim (const std::string s, const char* delims = " \t\v\f\r\n") {
	if(size_t pos = s.find_first_not_of(delims); pos != std::string::npos)
		return s.substr(pos);
	return ""; // Return a null string if we couldn't find any non-delims
}
// Function which removes all of the deliminating characters from the right side of a string
static std::string rtrim (const std::string& s, const char* delims = " \t\v\f\r\n") {
	if(size_t pos = s.find_last_not_of(delims); pos != std::string::npos)
		return s.substr(0, pos + 1);
	return ""; // Return a null string if we couldn't find any non-delims
}
// Function which removes all of the deliminating characters from the both sides of a string
inline std::string trim (const std::string& s, const char* delims = " \t\v\f\r\n") {
	return rtrim(ltrim(s, delims), delims);
}

int main(int argc, char* argv[]) {
    argh::parser args(argc, argv);

	// Parse command line
	std::vector<std::string> fileDirectories = split(args[1], ",");
	std::vector<std::string> ignoredEnds = split(args({"-i", "--ignored-suffixs"}).str(), ",");
	std::vector<std::string> templateDirectories = split(args({"-t", "--template-paths"}).str(), ",");
	size_t jobs = 1;
	try {
		std::stringstream jobsArg(args({"-j", "--jobs"}).str());
		jobsArg >> jobs;
	} catch(...) {}
	if(jobs < 1) jobs = 1;	
	
	// Extract file paths from the user provided directories
	std::vector<std::filesystem::path> files = recursivelyFindAndFilterFiles(fileDirectories, ignoredEnds);
	std::sort(files.begin(), files.end());
	std::vector<std::filesystem::path> templateFilePaths;
	// std::cout << templateDirectories.size() << std::endl;
	// std::cout << "`" << templateDirectories[0] << "`" << std::endl;
	if(!templateDirectories.empty())
		templateFilePaths = recursivelyFindAndFilterFiles(templateDirectories, ignoredEnds);

	// Load all of the template files into memory and unify their whitespace
	std::cout << "Loading template files:" << std::endl;
	std::vector<std::string> templateFiles;
	for(auto& path: templateFilePaths) {
		std::cout << path << std::endl;
		std::ifstream fin(path);
		templateFiles.emplace_back();
		std::getline(fin, templateFiles.back(), (char)EOF);
		templateFiles.back() = unifyWhitespace(templateFiles.back());
		fin.close();
	}
	std::cout << std::endl << std::endl;
	

	// Generate all of the possible unique pairs of submission files
	std::vector<std::pair<std::filesystem::path, std::filesystem::path>> filePairs;
	// for(size_t i = 0; i < files.size(); i++)
	// 	for(size_t j = i + 1; j < files.size(); j++)
	// 		if(files[i].string() != files[j].string())
	// 			filePairs.emplace_back(files[i], files[j]);
	auto indecies = findCombinationIndecies(files.size(), 2);
	for(auto& row: indecies)
		filePairs.emplace_back(files[row[0]], files[row[1]]);
	// for(auto& pair: filePairs)
	// 	std::cout << pair.first.string() << " <=> " << pair.second.string() << std::endl;

	std::mutex consoleLock;
	std::vector<std::thread> workThreads;
	for(int i = 0; i < jobs; i++)
		workThreads.emplace_back([id = i, jobs, &filePairs, &templateFiles, &consoleLock](){
			size_t size = filePairs.size() / jobs;
			for(size_t i = size * id; i < size * (id + 1); i++){
				const auto& pair = filePairs[i];

				std::string first;
				std::string second;

				{
					std::ifstream fin(pair.first);
					std::getline(fin, first, (char)EOF);
					first = unifyWhitespace(first);
					fin.close();
				}{
					std::ifstream fin(pair.second);
					std::getline(fin, second, (char)EOF);
					second = unifyWhitespace(second);
					fin.close();
				}

				dtl::Diff<std::string> diff(split(first, "\n"), split(second, "\n"));
				diff.onHuge();
				diff.compose();

				std::string commonLines;
				diff.composeUnifiedHunks();
				auto hunks = diff.getUniHunks();
				for(auto& hunk: hunks)
					for(auto& [common, info]: hunk.change)
						if(info.type == dtl::SES_COMMON) {						
							bool inTemplate = false;
							for(auto& templ: templateFiles)
								if(templ.find(common) != std::string::npos){
									inTemplate = true;
									break;
								}
							
							if(!inTemplate)
								commonLines += common + "\n";
							else if(!boost::iends_with(commonLines, "...\n"))
								commonLines += "...\n";
						} else if(!boost::iends_with(commonLines, "...\n"))
								commonLines += "...\n";

				// Ignore results where we found nothing in common
				if(commonLines == "...\n")
					continue;

				// Ignore single line results (unless they are very long)
				commonLines = trim(commonLines, " \t\b\r\n.");
				if(split(commonLines, "\n").size() < 2 && commonLines.size() < 50)
					continue;					

				if(commonLines.size() > 1){
					std::scoped_lock lock(consoleLock);
					std::cout << relative(pair.first).string() << " <=> " << relative(pair.second).string() << std::endl
						<< "`" << commonLines << "`" << std::endl << std::endl << std::endl;
				}

			}
		});

	// Wait for all of the threads to finish their work
	for(auto& thread: workThreads)
		thread.join();
	return 0;
}