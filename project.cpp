#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING 1;
#define _CRT_SECURE_NO_WARNINGS 1;
#include <iostream>
#include <windows.h>
#include <string>
#include <fstream>
#include <vector>
#include <random>
#include <locale>
#include <codecvt>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

int crashes_detected;

class jpgManager {
    std::string inputFile;
    std::string outputFile;
    int mutationCount;
public:
    jpgManager() {
        inputFile = ' ';
        outputFile = ' ';
        mutationCount = 0;
    };
    jpgManager(const std::string& i, const std::string& o, const int& c) {
        inputFile = i;
        outputFile = o;
        mutationCount = c;
    };
    void setIn(std::string in) {
        inputFile = in;
    }
    void setOut(const std::string& out) {
        outputFile = out;
    }
    void setMC(const int& n) {
        mutationCount = n;
    }
    void mutate() {
        std::ifstream inFile(inputFile, std::ios::binary);
        if (!inFile) {
            std::cerr << "[ERROR] Failed to open input file: " << inputFile << std::endl;
            return;
        }

        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            std::cerr << "[ERROR] Failed to open output file: " << outputFile << std::endl;
            return;
        }

        std::vector<unsigned char> buffer;
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<std::size_t> byteDist(0, 255);

        buffer.assign(std::istreambuf_iterator<char>(inFile), std::istreambuf_iterator<char>());

        for (std::size_t i = 0; i < mutationCount; ++i) {
            std::uniform_int_distribution<std::size_t> posDist(0, buffer.size() - 1);
            std::size_t mutationPos = posDist(rng);

            unsigned char mutationByte = static_cast<unsigned char>(byteDist(rng));
            buffer[mutationPos] = mutationByte;
        }

        outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }
};

class algorithm {
protected:
    virtual void execute() = 0;
};

class dumb_algorithm : algorithm {
    std::string programPath;
    std::string exampleQuery;
    int iteration_count;
    int current_mutation;
public:
    dumb_algorithm(std::string p, std::string q, int i, int m) : programPath(p), exampleQuery(q), iteration_count(i), current_mutation(m) {

    };
    void execute() {
        std::wstring wstrp(programPath.begin(), programPath.end());
        wchar_t* wcharprogramPath = new wchar_t[wstrp.size() + 1];
        std::wcscpy(wcharprogramPath, wstrp.c_str());

        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distr(15, 150);

        std::string exampleOutFile{};
        {
            fs::path examplepath(exampleQuery);
            std::string outfilename{};
            outfilename += std::to_string(++current_mutation);
            outfilename += ".jpg";
            fs::path eoutpath = examplepath.parent_path() / outfilename;
            exampleOutFile += eoutpath.string();
        }

        jpgManager mutationEngine(exampleQuery, exampleOutFile, distr(gen));
        mutationEngine.mutate();

        for (int i{}; i < iteration_count; ++i) {

            std::string exampleOutFile2 = " ";
            exampleOutFile2 += exampleOutFile;
            exampleOutFile2 += " ";
            std::wstring wstr(exampleOutFile2.begin(), exampleOutFile2.end());
            wchar_t* wchararguments = new wchar_t[wstr.size() + 1];
            std::wcscpy(wchararguments, wstr.c_str());



            if (!CreateProcess(
                wcharprogramPath,
                const_cast<wchar_t*>(wchararguments),
                NULL,
                NULL,
                FALSE,
                0,
                NULL,
                NULL,
                &si,
                &pi
            )) {
                printf("[ERROR] CreateProcess failed (%d).\n", GetLastError());
            }
            else {
                crashes_detected++;
                DWORD exitCode;
                if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0) {
                    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
                        std::cerr << "[UNEXPECTED] Failed to get exit code of child process. Saving file: " << current_mutation << ".jpg\n";
                    }
                    else if (exitCode == STATUS_ACCESS_VIOLATION) {
                        std::cout << "[CRASH] Child process encountered an access violation. Saving file: " << current_mutation << ".jpg\n";
                    }
                    else {
                        std::cout << "[PROCESS INFO] Child process exited with code: " << exitCode << std::endl;
                        crashes_detected--;
                        const fs::path filePath(exampleOutFile);
                        try {
                            fs::remove(filePath);
                        }
                        catch (const fs::filesystem_error& ex) {
                            std::cout << "[ERROR] Failed to remove the file: " << ex.what() << "\n";
                        }
                    }
                }
                else {
                    std::cerr << "[UNEXPECTED] Child process crashed or terminated unexpectedly. Saving file: " << current_mutation << ".jpg\n";
                }

                std::cout << i + 1 << " / " << iteration_count << std::endl;
                delete[] wchararguments;
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            {
                exampleOutFile = "";
                fs::path examplepath(exampleQuery);
                std::string outfilename{};
                outfilename += std::to_string(++current_mutation);
                outfilename += ".jpg";
                fs::path eoutpath = examplepath.parent_path() / outfilename;
                exampleOutFile += eoutpath.string();

                mutationEngine.setOut(exampleOutFile);
                mutationEngine.setMC(distr(gen));
                mutationEngine.mutate();
            }
        }
        std::cout << "crashes: " << crashes_detected << std::endl;
    }
};

class genetic_algorithm : algorithm {
    void execute() {

    }
};

class input_manager {
    std::string filename;
    std::string sample;
    unsigned int iteration_count;
    std::string algorithm;
    std::string logger_type;
    std::string logger_path;
public:
    input_manager(int argc, char** argv) {
        if (argc != 12) {
            std::cout << "Please, give all arguments:\n";
            std::cout << "-e <PATH> - PATH to your app\n";
            std::cout << "-s <PATH> - PATH to your example file to mutate\n";
            std::cout << "-i <INT> - Number of iterations to be performed\n";
            std::cout << "-a <GENETIC|DUMB> - Specify the algorithm to be used\n";
            std::cout << "-l <STD> <PATH> - Specify the logger to be used\n";
        }
        else {
            for (int i = 0; i < argc; ++i) {
                if (std::string(argv[i]) == "-e")
                    filename = std::string(argv[i + 1]);
                else if (std::string(argv[i]) == "-s")
                    sample = argv[i + 1];
                else if (std::string(argv[i]) == "-i")
                    iteration_count = stoi(std::string(argv[i + 1]));
                else if (std::string(argv[i]) == "-a") {
                    if (std::string(argv[i + 1]) != "GENETIC" && std::string(argv[i + 1]) != "DUMB") {
                        std::cout << "Available algorithms: GENETIC and DUMB\n";
                        return;
                    }
                    algorithm = argv[i + 1];
                }
                else if (std::string(argv[i]) == "-l") {
                    if (std::string(argv[i + 1]) != "JSON" && std::string(argv[i + 1]) != "STD") {
                        std::cout << "Available loggers: STD\n";
                        return;
                    }
                    logger_type = argv[i + 1];
                    logger_path = argv[i + 2];
                }
            }
        }
    }
    std::string get_filename() {
        return filename;
    }
    std::string get_sample() {
        return sample;
    }
    unsigned int get_iteration_count() {
        return iteration_count;
    }
    std::string get_algorithm() {
        return algorithm;
    }
    std::string get_logger_type() {
        return logger_type;
    }
    std::string get_logger_path() {
        return logger_path;
    }
};

class Fuzzer {
public:
    void run(int argc, char** argv) {
        input_manager i(argc, argv);
        std::string testedProgram = i.get_filename();
        std::string sampleFile = i.get_sample();
        int iterations = i.get_iteration_count();
        std::string algorithm = i.get_algorithm();
        std::string loggerType = i.get_logger_type();
        std::string loggerPath = i.get_logger_path();

        dumb_algorithm fuzzing(testedProgram, sampleFile, iterations, 0);
        fuzzing.execute();
    }
};

int main(int argc, char** argv)
{
    Fuzzer fuzzer;
    fuzzer.run(argc, argv);
}
