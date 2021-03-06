/**
 * @file ClickGenerator.h
 * @author Nico Van Looy (s0061909) & Jens De Wit (s0061864)
 * @brief Bachelor project of University of Antwerp - 2008-2009
 */

#ifndef CLICKGENERATOR_H_
#define CLICKGENERATOR_H_

//Standard
//Poco
//FireSim
#include "../parser/shorewall-parser/Table.h"
#include "../parser/network-parser/NetworkLayout.h"
#include "../parser/config-parser/ConfigParser.h"

class ClickGenerator {
public:

	/**
	 * Constructor
	 * @param config_path The directory where the configuration files are located.
	 * @isTestRun Indicates whether the run is a test run or not.
	 */
	ClickGenerator(std::string config_path, bool isTestRun);

	/**
	 * Destructor
	 */
	~ClickGenerator();

	/**
	 * Generate a simulation click script, based on the current shorewall parser.
	 * @param output This is the output stream. The generated click script will be printed to this stream.
	 */
	void generateSimulation(std::ostream& output);

private:
	void printTables(std::ostream& output);
	void printTrafficTypes(std::ostream& output);
	void printTrafficSwitch(std::ostream& output);

	Table* _filterTable;
	Table* _natTable;
	Table* _mangleTable;
	NetworkLayout* _networkLayout;
	ConfigParser* _configParser;
	int _numTrafficBlocks;
	bool _testRun;
};
#endif /* CLICKGENERATOR_H_ */
