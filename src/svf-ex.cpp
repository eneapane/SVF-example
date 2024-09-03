//===- svf-ex.cpp -- A driver example of SVF-------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===-----------------------------------------------------------------------===//

/*
 // A driver program of SVF including usages of SVF APIs
 //
 // Author: Yulei Sui,
 */

#include "SVF-LLVM/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "WPA/Andersen.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/Options.h"
#include <fstream>
#include <filesystem>
#include <vector>
#include <sstream>
#include <regex>
#include <typeinfo>

using namespace llvm;

using namespace std;
using namespace SVF;
namespace fs = std::filesystem;

// Function to extract the function name from the given string
std::string extractFunctionName(const std::string &input) {
    std::regex pattern(R"(Function:\s+(\w+))");
    std::smatch matches;

    if (std::regex_search(input, matches, pattern) && matches.size() > 1) {
        return matches[1].str();
    } else {
        return "";
    }
}


std::string extractType(const std::string &str) {
    std::istringstream iss(str);
    std::string type;
    iss >> type; // Extract the first word, which is the type
    return type;
}

std::string extractArgName(const std::string &str) {
    std::istringstream iss(str);
    std::string temp, argName;

    iss >> temp >> argName;

    // Remove the leading '%' from the argument name
    if (!argName.empty() && argName[0] == '%') {
        argName = argName.substr(1);
    }

    return argName;
}

std::string extractFileLocation(const std::string &str) {
    std::string fileKey = "\"file\":";
    size_t pos = str.find(fileKey);

    if (pos != std::string::npos) {
        // Move position past the "file": keyword and any whitespace
        pos += fileKey.length();
        while (pos < str.length() && isspace(str[pos])) {
            pos++;
        }

        // Check if the file path is wrapped in quotes
        if (str[pos] == '"') {
            pos++;  // Skip the opening quote
            size_t endPos = str.find('"', pos);
            if (endPos != std::string::npos) {
                return str.substr(pos, endPos - pos);
            }
        }
    }

    // Return empty string if "file": not found or any other error occurs
    return "";
}

void dump_points_to(const SVFModule *svfModule, SVFIR *pag, Andersen *ander, const std::string &filename) {
    // Open the file for writing
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file: " + filename << std::endl;
        return;
    }


    //outFile << "SIZE: " << svfModule->getFunctionSet().size() << std::endl;
    int num_functions = svfModule->getFunctionSet().size();
    outFile << "{\n";
    int j = 0;
    for (const auto &function: svfModule->getFunctionSet()) {
        j++;
        NodeID returnNode = pag->getReturnNode(function);
        auto pts = ander->getPts(returnNode);
        outFile << "\t" << "\"" << extractFunctionName(function->toString()) << "\": " << "{\n";

        int result_set_size = 0;
        for (const auto &nodeId: pts) {
            result_set_size++;
            auto node = pag->getGNode(nodeId);
            //outFile << node->toString() << ", ";
        }
        outFile << "\t\t\"result_set_size\": \"" << result_set_size << "\",\n";


        int argument_num = 0;
        outFile << "\t\t\"arguments_set_sizes\": {\n";
        if (pag->getFunArgsMap().find(function) != pag->getFunArgsMap().end()) {
            for (const auto &item: pag->getFunArgsList(function)) {
                auto pts2 = ander->getPts(item->getId());
                outFile << "\t\t\t\"arg_" << argument_num << "\": \"" << pts2.count() << "\"";
                if (argument_num < pag->getFunArgsList(function).size() - 1)
                    outFile << ',';
                outFile << '\n';

                for (const auto &nodeId: pts2) {
                    auto node = pag->getGNode(nodeId);
                    //outFile << "\t\t" << node->toString() << ',\n';
                }
                argument_num++;
            }
        }
        outFile << "\t\t},\n";
        outFile << "\t\t\"num_arguments\": \"" << argument_num;
        outFile << "\"\n\t}";
        //outFile << "\t}\n}";
        if (j <= num_functions - 1)
            outFile << ',';
        outFile << endl;
    }
    outFile << "}\n";

    outFile.close();
}

void dump_arguments_compare(const SVFModule *svfModule, SVFIR *pag, Andersen *ander, const std::string &filename) {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file: " + filename << std::endl;
        return;
    }

    outFile << "{\n";
    int funct_num = 0;
    for (const auto &function: svfModule->getFunctionSet()) {
        funct_num++;
        outFile << "\t" << "\"" << extractFunctionName(function->toString()) << "\": " << "[\n";
        for (int i = 0; i < function->arg_size(); i++) {
            outFile << "\t\t{" << endl;
            const SVFType *type = function->getArg(i)->getType();
            const std::string current_argument = function->getArg(i)->toString();
            outFile << "\t\t\t\"type\": \"" << extractType(current_argument) << "\"," << endl;
            outFile << "\t\t\t\"name\": \"" << extractArgName(current_argument) << "\"," << endl;
            outFile << "\t\t\t\"file\": \"" << extractFileLocation(current_argument) << "\"" << endl;
            outFile << "\t\t}";
            if (i < function->arg_size() - 1) {
                outFile << ", ";
            }
            outFile << endl;
        }
        outFile << "\t]";
        if (funct_num <= svfModule->getFunctionSet().size() - 1)
            outFile << ',';
        outFile << endl;
    }
    outFile << "}\n";
    outFile.close();
}

std::vector<std::string> split(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

int main(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        std::cout << "Argument " << i << ": " << argv[i] << std::endl;
    }

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
            argc, argv, "Whole Program Points-to Analysis", "[options] <input-bitcode...>"
    );

    if (Options::WriteAnder() == "ir_annotator") {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    SVFModule *svfModule = LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVFIRBuilder builder(svfModule);
    SVFIR *pag = builder.build();

    Andersen *ander = AndersenWaveDiff::createAndersenWaveDiff(pag);

    std::string file_path = argv[argc - 1]; //always given as the last command line parameter
    std::vector<std::string> parts = split(file_path, '/');
    const std::string &subdirectory = parts[parts.size() - 2];
    std::string prefix;
    std::string suffix = split(parts.back(), '.')[0];
    if (subdirectory == "llvm") {
        prefix = "source";
    } else {
        std::string lastTwoChars = subdirectory.substr(subdirectory.length() - 2);
        prefix = lastTwoChars;
    }
    std::string directory = "/app/output/reports/";
    if (!fs::exists(directory)) {
        if (!fs::create_directories(directory)) {
            std::cerr << "Failed to create directory: " << directory << std::endl;
        }
    }
    if (std::string(argv[1]) != "-brief-constraint-graph") {
        dump_points_to(svfModule, pag, ander, directory + prefix + "_points_to_analysis_" + suffix);
        dump_arguments_compare(svfModule, pag, ander, directory + prefix + "_arguments_comparison_" + suffix);
    } else {
        PTACallGraph *callgraph = ander->getPTACallGraph();
        callgraph->dump(directory + prefix + "_call_graph_" + suffix);
    }

    AndersenWaveDiff::releaseAndersenWaveDiff();
    SVFIR::releaseSVFIR();

    LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    SVF::LLVMModuleSet::releaseLLVMModuleSet();

    llvm::llvm_shutdown();
    return 0;
}

