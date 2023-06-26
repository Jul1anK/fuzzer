#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING 1;
#define _CRT_SECURE_NO_WARNINGS 1;
#include <wx/spinctrl.h>
#include <wx/wx.h>
#include <iostream>
#include <vector>
#include <windows.h>
#include <string>
#include <fstream>
#include <random>
#include <locale>
#include <codecvt>
#include <experimental/filesystem>
#include <thread>
#include <regex>

namespace fs = std::experimental::filesystem;

int crashes_detected;

class Logger {
    static void log(const std::string& message) {
        std::ofstream logFile("log.txt", std::ios::app);
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&currentTime));
            std::cout << "[" << buffer << "] " << message << std::endl;
            logFile << "[" << buffer << "] " << message << std::endl;
            logFile.close();
        }
    }
public:
    static void logError(const std::string& message, std::regex regex) {
        std::string logmessage = "[ERROR] " + message;
        if(std::regex_search(logmessage, regex))
            log(logmessage);
    }
    static void logUnexpected(const std::string& message, std::regex regex) {
        std::string logmessage = "[UNEXPECTED] " + message;
        if (std::regex_search(logmessage, regex))
            log(logmessage);
    }
    static void logCrash(const std::string& message, std::regex regex) {
        std::string logmessage = "[CRASH] " + message;
        if (std::regex_search(logmessage, regex))
            log(logmessage);
    }
    static void logProcessInfo(const std::string& message, std::regex regex) {
        std::string logmessage = "[PROCESS INFO] " + message;
        if (std::regex_search(logmessage, regex))
            log(logmessage);
    }
};

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
    void mutate(std::regex regex) {
        std::ifstream inFile(inputFile, std::ios::binary);
        if (!inFile) {
            Logger::logError("Failed to open input file: " + inputFile, regex);
            return;
        }

        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            Logger::logError("Failed to open input file: " + outputFile, regex);
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
    virtual void execute(std::regex regex) = 0;
};

class dumb_algorithm : algorithm {
    std::string programPath;
    std::string exampleQuery;
    int iteration_count;
    int current_mutation;
public:
    dumb_algorithm(std::string p, std::string q, int i, int m) : programPath(p), exampleQuery(q), iteration_count(i), current_mutation(m) {

    };
    void execute(std::regex regex) {
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
        mutationEngine.mutate(regex);

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
                Logger::logError("CreateProcess failed " + GetLastError(), regex);
            }
            else {
                crashes_detected++;
                DWORD exitCode;
                if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0) {
                    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
                        Logger::logUnexpected("Failed to get exit code of the process. Saving file: " + std::to_string(current_mutation) + ".jpg", regex);
                    }
                    else if (exitCode == STATUS_ACCESS_VIOLATION) {
                        Logger::logCrash("Process encountered an access violation. Saving file: " + std::to_string(current_mutation) + ".jpg", regex);
                    }
                    else {
                        Logger::logProcessInfo("Process exited with code: " + exitCode, regex);
                        crashes_detected--;
                        const fs::path filePath(exampleOutFile);
                        try {
                            fs::remove(filePath);
                        }
                        catch (const fs::filesystem_error& ex) {
                            Logger::logError("Failed to remove the file: " + exampleOutFile, regex);
                        }
                    }
                }
                else {
                    Logger::logUnexpected("Process crashed or terminated unexpectedly. Saving file: " + std::to_string(current_mutation) + ".jpg", regex);
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
                mutationEngine.mutate(regex);
            }
        }
    }
};


class dumb_algorithm_th : algorithm {
    std::string programPath;
    std::string exampleQuery;
    int iteration_count;
    int current_mutation;
    void checkForCrash(wchar_t* wcharprogramPath, std::string exampleOutFile, jpgManager mutationEngine, STARTUPINFO si, PROCESS_INFORMATION pi, int a, int i, std::regex regex) {
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
                    std::cerr << "[UNEXPECTED] Failed to get exit code of the process. Saving file: " << current_mutation << ".jpg\n";
                }
                else if (exitCode == STATUS_ACCESS_VIOLATION) {
                    std::cout << "[CRASH] Process encountered an access violation. Saving file: " << current_mutation << ".jpg\n";
                }
                else {
                    std::cout << "[PROCESS INFO] Process exited with code: " << exitCode << std::endl;
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
                std::cerr << "[UNEXPECTED] Process crashed or terminated unexpectedly. Saving file: " << current_mutation << ".jpg\n";
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
            mutationEngine.setMC(a);
            mutationEngine.mutate(regex);
        }
    }
public:
    dumb_algorithm_th(std::string p, std::string q, int i, int m) : programPath(p), exampleQuery(q), iteration_count(i), current_mutation(m) {};
    void execute(std::regex regex) {
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
        mutationEngine.mutate(regex);

        int numThreads = 4;
        std::vector<std::thread> threads;
        for (int i{}; i < iteration_count / numThreads; ++i) {
            for (int j = 0; j < numThreads; ++j) {
                int a = distr(gen);
                int b = i * numThreads + j;
                threads.emplace_back([&, wcharprogramPath, exampleOutFile, mutationEngine, si, pi, a, b]() {
                    checkForCrash(wcharprogramPath, exampleOutFile, mutationEngine, si, pi, a, b, regex);
                    });
            }
        }
        for (std::thread& thread : threads) {
            thread.join();
        }
    }
};


class genetic_algorithm : algorithm {
private:
    std::string programPath;
    std::string exampleQuery;
    int crashnum;

    bool hasCrashed(const std::string& inputFile) {
        std::cout << crashnum + 1 << std::endl;
        std::wstring wstrp(programPath.begin(), programPath.end());
        wchar_t* wcharprogramPath = new wchar_t[wstrp.size() + 1];
        std::wcscpy(wcharprogramPath, wstrp.c_str());

        std::string exampleOutFile2 = " ";
        exampleOutFile2 += inputFile;
        exampleOutFile2 += " ";
        std::wstring wstr(exampleOutFile2.begin(), exampleOutFile2.end());
        wchar_t* wchararguments = new wchar_t[wstr.size() + 1];
        std::wcscpy(wchararguments, wstr.c_str());

        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::string exampleOutFile{};
        {
            fs::path examplepath(inputFile);
            std::string outfilename{};
            outfilename += std::to_string(++crashnum);
            outfilename += ".jpg";
            fs::path eoutpath = examplepath.parent_path() / outfilename;
            exampleOutFile += eoutpath.string();
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distr(15, 150);

        /*jpgManager mutationEngine(inputFile, exampleOutFile, distr(gen));
        mutationEngine.mutate();*/

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
            printf("CreateProcess failed (%d).\n", GetLastError());
            return 0;
        }
        crashes_detected++;
        DWORD exitCode;
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0) {
            if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
                std::cerr << "Failed to get exit code of child process." << std::endl;
                return 1;
            }
            else if (exitCode == STATUS_ACCESS_VIOLATION) {
                std::cout << "Child process encountered an access violation." << std::endl;
                return 1;
            }
            else {
                std::cout << "Child process exited with code: " << exitCode << std::endl;
                crashes_detected--;
                const fs::path filePath(exampleOutFile);
                try {
                    fs::remove(filePath);
                }
                catch (const fs::filesystem_error& ex) {
                    std::cout << "[ERROR] Failed to remove the file: " << ex.what() << "\n";
                }
                return 0;
            }
        }
        else {
            std::cerr << "Child process crashed or terminated unexpectedly." << std::endl;
            return 1;
        }
    }
    void crossover(const std::string& file1Path, const std::string& file2Path, const std::string& outputPath) {
        std::ifstream file1(file1Path, std::ios::binary);
        std::ifstream file2(file2Path, std::ios::binary);
        std::ofstream output(outputPath, std::ios::binary);

        if (!file1 || !file2 || !output) {
            //std::cerr << "Error opening files." << std::endl;
            return;
        }
        const int headerSize = 16;
        std::vector<char> headerBuffer(headerSize);
        file1.read(headerBuffer.data(), headerSize);
        output.write(headerBuffer.data(), headerSize);

        std::vector<char> file1Data((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
        std::vector<char> file2Data((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
        const int crossoverPoint = std::rand() % (file1Data.size() - headerSize) + headerSize;
        output.write(file1Data.data() + headerSize, crossoverPoint - headerSize);
        output.write(file2Data.data() + crossoverPoint, file2Data.size() - crossoverPoint);

        file1.close();
        file2.close();
        output.close();
    }
public:
    genetic_algorithm(const std::string& i, const std::string& q, int n) : programPath(i), exampleQuery(q), crashnum(n) {};
    void execute(std::regex regex) {
        const int POPULATION_SIZE = 10;
        const int MAX_GENERATIONS = 100;
        const double MUTATION_RATE = 0.1;
        const double CROSSOVER_RATE = 0.8;

        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        std::vector<std::string> population(POPULATION_SIZE);
        for (auto& inputFile : population) {
            int n = 10;
            fs::path examplepath(exampleQuery);
            std::string outfilename{};
            outfilename += std::to_string(n);
            outfilename += ".jpg";
            fs::path eoutpath = examplepath.parent_path() / outfilename;
            inputFile += eoutpath.string();
            jpgManager mutationEngine(exampleQuery, inputFile, 30);
            mutationEngine.mutate(regex);
        }

        int generation{};
        while (generation < MAX_GENERATIONS) {
            std::vector<std::string> nextGeneration;
            std::vector<bool> crashedPopulation(POPULATION_SIZE, false);
            for (int i = 0; i < POPULATION_SIZE; ++i) {
                if (hasCrashed(population[i])) {
                    std::string a;
                    std::cin >> a;
                    crashedPopulation[i] = true;
                }
            }
            for (int i = 0; i < POPULATION_SIZE; ++i) {
                if (crashedPopulation[i]) {
                    continue;
                }
                for (int j = i + 1; j < POPULATION_SIZE; ++j) {
                    if (crashedPopulation[j]) {
                        continue;
                    }
                    if (static_cast<float>(std::rand()) / RAND_MAX < CROSSOVER_RATE) {
                        std::string offspring;
                        crossover(population[i], population[j], offspring);
                        nextGeneration.push_back(offspring);
                    }
                }
            }
            for (auto& individual : nextGeneration) {
                if (static_cast<double>(std::rand()) / RAND_MAX < MUTATION_RATE) {
                    jpgManager mutationEngine(individual, individual, 15);
                    mutationEngine.mutate(regex);
                }
            }
            population = std::move(nextGeneration);
            ++generation;
        }
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
    /*void run(int argc, char** argv) {
        input_manager i(argc, argv);
        std::string testedProgram = i.get_filename();
        std::string sampleFile = i.get_sample();
        int iterations = i.get_iteration_count();
        std::string algorithm = i.get_algorithm();

        dumb_algorithm fuzzing(testedProgram, sampleFile, iterations, 0);
        fuzzing.execute();
    }*/

    void gui_run(std::string pp, std::string fp, int i, std::string a, std::string l, std::regex regex) {
        dumb_algorithm fuzzing(pp, fp, i, 0);
        fuzzing.execute(regex);
    }
};
enum class AlgorithmType
{
    RANDOM,
    GENETIC
};

enum class LoggerType
{
    STD
};

class MainFrame : public wxFrame
{
public:
    MainFrame(const wxString& title)
        : wxFrame(NULL, wxID_ANY, title)
    {
        // Create the GUI controls
        wxPanel* panel = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* programLabel = new wxStaticText(panel, wxID_ANY, "Program Path:");
        programTextCtrl = new wxTextCtrl(panel, wxID_ANY);
        wxStaticText* sampleLabel = new wxStaticText(panel, wxID_ANY, "Sample File Path:");
        sampleTextCtrl = new wxTextCtrl(panel, wxID_ANY);
        wxStaticText* iterationsLabel = new wxStaticText(panel, wxID_ANY, "Iterations:");
        iterationsSpinCtrl = new wxSpinCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, INT_MAX, 1);
        wxStaticText* algorithmLabel = new wxStaticText(panel, wxID_ANY, "Algorithm Type:");
        algorithmChoice = new wxChoice(panel, wxID_ANY);
        wxStaticText* loggerLabel = new wxStaticText(panel, wxID_ANY, "Logger Type:");
        loggerChoice = new wxChoice(panel, wxID_ANY);
        wxStaticText* logFilterLabel = new wxStaticText(panel, wxID_ANY, "Log Filter:");
        errorCheckBox = new wxCheckBox(panel, wxID_ANY, "Error");
        processInfoCheckBox = new wxCheckBox(panel, wxID_ANY, "Process Info");
        unexpectedCheckBox = new wxCheckBox(panel, wxID_ANY, "Unexpected");
        crashCheckBox = new wxCheckBox(panel, wxID_ANY, "Crash");
        logButton = new wxButton(panel, wxID_ANY, "Run");
        logTextCtrl = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

        // Add the controls to the sizer
        sizer->Add(programLabel, 0, wxALL, 5);
        sizer->Add(programTextCtrl, 0, wxALL, 5);
        sizer->Add(sampleLabel, 0, wxALL, 5);
        sizer->Add(sampleTextCtrl, 0, wxALL, 5);
        sizer->Add(iterationsLabel, 0, wxALL, 5);
        sizer->Add(iterationsSpinCtrl, 0, wxALL, 5);
        sizer->Add(algorithmLabel, 0, wxALL, 5);
        sizer->Add(algorithmChoice, 0, wxALL, 5);
        sizer->Add(loggerLabel, 0, wxALL, 5);
        sizer->Add(loggerChoice, 0, wxALL, 5);
        sizer->Add(logFilterLabel, 0, wxALL, 5);
        sizer->Add(errorCheckBox, 0, wxALL, 5);
        sizer->Add(processInfoCheckBox, 0, wxALL, 5);
        sizer->Add(unexpectedCheckBox, 0, wxALL, 5);
        sizer->Add(crashCheckBox, 0, wxALL, 5);
        sizer->Add(logButton, 0, wxALL, 5);
        sizer->Add(logTextCtrl, 1, wxEXPAND | wxALL, 5);

        panel->SetSizer(sizer);

        // Bind events
        logButton->Bind(wxEVT_BUTTON, &MainFrame::OnLogButtonClicked, this);

        // Populate algorithm choice
        algorithmChoice->Append("RANDOM");
        algorithmChoice->Append("GENETIC");

        // Populate logger choice
        loggerChoice->Append("STD");
    }

private:
    void OnLogButtonClicked(wxCommandEvent& event)
    {
        wxString programPath = programTextCtrl->GetValue();
        wxString sampleFilePath = sampleTextCtrl->GetValue();
        int iterationsNumber = iterationsSpinCtrl->GetValue();
        wxString algorithmType = algorithmChoice->GetString(algorithmChoice->GetSelection());
        wxString loggerType = loggerChoice->GetString(loggerChoice->GetSelection());

        wxString logsOfInterest = "NOT_MATCHING";
        if (errorCheckBox->GetValue())
            logsOfInterest += "|ERROR";
        if (processInfoCheckBox->GetValue())
            logsOfInterest += "|PROCESS INFO";
        if (unexpectedCheckBox->GetValue())
            logsOfInterest += "|UNEXPECTED";
        if (crashCheckBox->GetValue())
            logsOfInterest += "|CRASH";
        
        std::regex regex(logsOfInterest);
        Fuzzer fuzzer;
        fuzzer.gui_run(programPath.ToStdString(), sampleFilePath.ToStdString(), iterationsNumber, algorithmType.ToStdString(), loggerType.ToStdString(), regex);

        wxString output = wxString::Format("Program Path: %s\nSample File Path: %s\nIterations: %d\nAlgorithm Type: %s\nLogger Type: %s\nLogs of Interest: %s\nCrashes detected: %d",
            programPath, sampleFilePath, iterationsNumber, algorithmType, loggerType, logsOfInterest, crashes_detected);

        logTextCtrl->SetValue(output);
    }

    wxTextCtrl* programTextCtrl;
    wxTextCtrl* sampleTextCtrl;
    wxSpinCtrl* iterationsSpinCtrl;
    wxChoice* algorithmChoice;
    wxChoice* loggerChoice;
    wxCheckBox* errorCheckBox;
    wxCheckBox* processInfoCheckBox;
    wxCheckBox* unexpectedCheckBox;
    wxCheckBox* crashCheckBox;
    wxButton* logButton;
    wxTextCtrl* logTextCtrl;

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
EVT_BUTTON(wxID_ANY, MainFrame::OnLogButtonClicked)
wxEND_EVENT_TABLE()

class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};

bool MyApp::OnInit()
{
    MainFrame* frame = new MainFrame("Logger GUI");
    frame->Show(true);
    return true;
}

wxIMPLEMENT_APP(MyApp);
