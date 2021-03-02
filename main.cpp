#include <iostream>
#include <string>
#include <vector>

#include "Computer/Computer.h"
#include "Transmission/Transmission.h"

struct Config
{
    size_t NumberComputer = 2;
    size_t FirstNumber = 1;
    std::string GlobalConfigName = "";
};

Config parse_arguments(int argc, char *argv[])
{
    Config config;
    config.NumberComputer = 2;

    for (int i = 1; i < argc; ++i)
    {
        char* arg = argv[i];
        if (std::string(arg) == "-c")
        {
            if (i + 1 < argc)
            {
                std::cout << "Initialization avec " << argv[i+1] << " ordinateurs." << std::endl;
                int numberComputer = std::stoi(argv[i + 1]);
                if (numberComputer < 1)
                {
                    std::cout << "Le nombre d'ordinateur doit etre superieur a 1." << std::endl;
                }
                else
                {
                    config.NumberComputer = (size_t)numberComputer;
                }
            }
            else
            {
                std::cout << "Le parametre -c doit etre suivi du nombre d'ordinateur a mettre en connexion." << std::endl;
            }
        }
        else if (std::string(arg) == "-f")
        {
            if (i + 1 < argc)
            {
                std::cout << "Premier numero des ordinateurs : " << argv[i+1] << std::endl;
                int firstNumber = std::stoi(argv[i + 1]);
                if (firstNumber < 0)
                {
                    std::cout << "Le premier numero doit etre superieur a 0." << std::endl;
                }
                else
                {
                    config.FirstNumber = (size_t)firstNumber;
                }
            }
            else
            {
                std::cout << "Le parametre -f doit etre suivi du numero du premier ordinateur de la simulation." << std::endl;
            }
        }
        else if (std::string(arg) == "-g")
        {
            if (i + 1 < argc)
            {
                std::cout << "Configuration globale a utiliser : " << argv[i + 1] << std::endl;
                config.GlobalConfigName = std::string(argv[i+1]);
            }
            else
            {
                std::cout << "Le parametre -g doit etre suivi du nom de fichier de configuration principal de la simulation." << std::endl;
            }
        }
    }

    return config;
}

int main(int argc, char *argv[])
{
    Config config = parse_arguments(argc, argv);
    Configuration globalConfig(config.GlobalConfigName);

    std::cout << "Demarrage du simulateur..." << std::endl;

    TransmissionHub hub(globalConfig);

    std::vector<Computer*> computers;
    // Create computer and connect it to Hub
    for (size_t i = 0; i < config.NumberComputer; ++i)
    {
        Computer* computer = new Computer(i+config.FirstNumber);
        hub.connect_computer(computer);
        computers.push_back(computer);
    }
    hub.start();

    for (auto computer : computers)
    {
        computer->start();
    }

    unsigned int numbercomputerFinished = 0;
    unsigned int numberFileReceived = 0;
    unsigned int numberFileSent = 0;
    do
    {
        numbercomputerFinished = 0;
        numberFileReceived = 0;
        numberFileSent = 0;
        for (const Computer* computer : computers)
        {
            if (computer->sendingTerminated())
            {
                ++numbercomputerFinished;
            }
            numberFileSent += computer->sentFileCount();
            numberFileReceived += computer->receivedFileCount();
        }
    } while (numbercomputerFinished != computers.size() || numberFileReceived != numberFileSent);
    

    std::cout << "Arret du simulateur..." << std::endl;
    hub.stop();
    for (auto computer : computers)
    {
        delete computer;
    }
    computers.clear();
}