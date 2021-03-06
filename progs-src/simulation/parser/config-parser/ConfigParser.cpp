/**
 * @file ConfigParser.cpp
 * @author Nico Van Looy (s0061909) & Jens De Wit (s0061864)
 * @brief Bachelor project of University of Antwerp - 2008-2009
 */

//Standard
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "assert.h"

//Poco
#include "Poco/DOM/Node.h"
#include "Poco/Exception.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Logger.h"

//Firesim
#include "ConfigParser.h"
#include "XmlTagConstants.h"
#include "ProtocolFactory.h"
#include "PolicyFactory.h"
#include "../ApplicationConstants.h"
#include "../util/Util.h"

using Poco::XML::Node;
using Poco::Logger;

/**
 * Initialization of static members.
 */
ConfigParser* ConfigParser::fInstance = 0;

//forward declaration of helper function
void checkDump(std::pair<std::string, bool> dump, std::string outInterface, std::string inInterface, NetworkLayout* networkLayout);
bool fileExist(std::string path);

ConfigParser::ConfigParser(std::string path): _path(path) {
}

void ConfigParser::createInstance(std::string path) {
	if (fInstance == 0) {
		fInstance = new ConfigParser(path);
	}
}

ConfigParser* ConfigParser::getInstance() {
	assert(fInstance);
	return fInstance;
}

int ConfigParser::parse(Poco::XML::Document* xmlDocument, NetworkLayout* networkLayout) {
	Node* configNode = xmlDocument->firstChild();
	if(!configNode || (configNode->nodeName()!=XMLConstants::CONFIG_TAG)){
		std::cout << "config.xml: No element with name " << XMLConstants::CONFIG_TAG << " at the appropiate place." << std::endl;
		exit(1);
	}
	Node* trafficNode = configNode->firstChild();
	if(!trafficNode || (trafficNode->nodeName()!=XMLConstants::TRAFFIC_TAG)){
		std::cout << "config.xml: No element with name " << XMLConstants::TRAFFIC_TAG << " at the appropiate place." << std::endl;
		exit(1);
	}

	int numTrafficNodes = 0;

	//For each traffic block
	while (trafficNode != 0) {
		ConfigContent* content = new ConfigContent();

		numTrafficNodes++;

		//initialize non-mandatory content
		content->tcp_flags = std::vector<Flags>();
		content->tcp_flags.push_back(SYN);
		content->icmp_type = 0;
		content->icmp_code = -1;
		content->dumpPath = std::pair<std::string, bool>("", false);
		content->destIP = IPAddress();
		content->sourceIP = IPAddress();
		content->inInterface = "";
		content->outInterface = "";
		content->numPackets = 1;

		Node* elementNode = trafficNode->firstChild();
		while (elementNode != 0){

			//elementNode must be an element node with name XMLConstants::SOURCEIPTAG or XMLConstants::SOURCEPORTTAG or ...
			if(		   (elementNode->nodeName()!=XMLConstants::SOURCE_IP_TAG)
					&& (elementNode->nodeName()!=XMLConstants::SOURCE_PORT_TAG)
					&& (elementNode->nodeName()!=XMLConstants::DESTINATION_IP_TAG)
					&& (elementNode->nodeName()!=XMLConstants::DESTINATION_PORT_TAG)
					&& (elementNode->nodeName()!=XMLConstants::PROTOCOL_TAG)
					&& (elementNode->nodeName()!=XMLConstants::TCP_FLAGS_TAG)
					&& (elementNode->nodeName()!=XMLConstants::ICMP_TYPE)
					&& (elementNode->nodeName()!=XMLConstants::ICMP_CODE)
					&& (elementNode->nodeName()!=XMLConstants::POLICY_TAG)
					&& (elementNode->nodeName()!=XMLConstants::IN_INTERFACE_TAG)
					&& (elementNode->nodeName()!=XMLConstants::OUT_INTERFACE_TAG)
					&& (elementNode->nodeName()!=XMLConstants::DUMP_TAG)
					&& (elementNode->nodeName()!=XMLConstants::ASCII_DUMP_TAG)
					&& (elementNode->nodeName()!=XMLConstants::NUM_PACKETS)){
				std::cout << "config.xml: XML element " << elementNode->nodeName() << " is not supported." << std::endl;
				exit(1);
			}

			Node* textNode = elementNode->firstChild();

			if(!textNode){
				std::cout << "config.xml: XML elements must contain something else than whitespace."<<std::endl;
				exit(1);
			}

			if (elementNode->nodeName() == XMLConstants::SOURCE_IP_TAG){
				std::string ip = textNode->nodeValue();
				removeWhitespace(ip);
				try {
					content->sourceIP = IPAddress(ip);
				} catch (Poco::Exception e) {
					std::cout << "config.xml: Parsed faulty source IP address" << ip << ": '" << e.message() << "'. Terminating." << std::endl;
					exit(1);
				}
			} else if (elementNode->nodeName() == XMLConstants::SOURCE_PORT_TAG) {
				if ((content->protocol != UDP) && (content->protocol != TCP)){
					std::cout << "config.xml: You can only specify a source port if you have already specified the protocol as tcp or udp." << std::endl;
					exit(1);
				}
				content->sourcePort = atoi(textNode->nodeValue().c_str());
			} else if (elementNode->nodeName() == XMLConstants::DESTINATION_IP_TAG) {
				std::string ip = textNode->nodeValue();
				removeWhitespace(ip);
				try {
					content->destIP = IPAddress(ip);
				} catch (Poco::Exception e) {
					std::cout << "config.xml: Parsed faulty destination IP address " << ip << ": '" << e.message() << "'. Terminating." << std::endl;
					exit(1);
				}
			} else if (elementNode->nodeName() == XMLConstants::DESTINATION_PORT_TAG) {
				if ((content->protocol != UDP) && (content->protocol != TCP)){
					std::cout << "config.xml: You can only specify a destination port if you have already specified the protocol as tcp or udp." << std::endl;
					exit(1);
				}
				content->destPort = atoi(textNode->nodeValue().c_str());
			} else if (elementNode->nodeName() == XMLConstants::PROTOCOL_TAG) {
				ProtocolFactory* factory = ProtocolFactory::getInstance();
				content->protocol = factory->parse(textNode->nodeValue());
			} else if (elementNode->nodeName() == XMLConstants::TCP_FLAGS_TAG) {
				if (content->protocol != TCP){
					std::cout << "config.xml: You can only specify tcp flags if you have already specified the protocol as tcp." << std::endl;
					exit(1);
				}

				Node* flagNode = elementNode->firstChild();
				while (flagNode != 0) {
					Node* textNode = flagNode->firstChild();

					if(!textNode){
						std::cout << "config.xml: XML elements must contain something else than whitespace."<<std::endl;
						exit(1);
					}

					std::string value = textNode->getNodeValue();
					removeWhitespace(value);
					if(value == "1") {
						if (flagNode->nodeName()==XMLConstants::URG_FLAG_TAG) {
							content->tcp_flags.push_back(URG);
						} else if (flagNode->nodeName()==XMLConstants::ACK_FLAG_TAG) {
							content->tcp_flags.push_back(ACK);
						} else if (flagNode->nodeName()==XMLConstants::PSH_FLAG_TAG) {
							content->tcp_flags.push_back(PSH);
						} else if (flagNode->nodeName()==XMLConstants::RST_FLAG_TAG) {
							content->tcp_flags.push_back(RST);
						} else if (flagNode->nodeName()==XMLConstants::SYN_FLAG_TAG) {
							content->tcp_flags.push_back(SYN);
						} else if (flagNode->nodeName()==XMLConstants::FIN_FLAG_TAG) {
							content->tcp_flags.push_back(FIN);
						} else {
							std::cout << "config.xml: XML element " << flagNode->nodeName() << " is not supported." << std::endl;
							exit(1);
						}
					} else if (value == "0") {
						if (flagNode->nodeName()==XMLConstants::SYN_FLAG_TAG) {
							for (std::vector<Flags>::iterator it = content->tcp_flags.begin(); it != content->tcp_flags.end(); it++) {
								if (*it == SYN) {
									content->tcp_flags.erase(it);
								}
							}

						}
					} else if ((flagNode->nodeName()!=XMLConstants::URG_FLAG_TAG)
								&& (flagNode->nodeName()!=XMLConstants::ACK_FLAG_TAG)
								&& (flagNode->nodeName()!=XMLConstants::PSH_FLAG_TAG)
								&& (flagNode->nodeName()!=XMLConstants::RST_FLAG_TAG)
								&& (flagNode->nodeName()!=XMLConstants::SYN_FLAG_TAG)
								&& (flagNode->nodeName()!=XMLConstants::FIN_FLAG_TAG)){
						std::cout << "config.xml: XML element " << flagNode->nodeName() << " is not supported as a tcp flag." << std::endl;
						exit(1);
					} else {
						std::cout << "config.xml: Content of XML element " << flagNode->nodeName() << " should be 0 or 1." << std::endl;
						exit(1);
					}

					flagNode = flagNode->nextSibling();
				}
			} else if (elementNode->nodeName() == XMLConstants::ICMP_TYPE) {
				if (content->protocol != ICMP){
					std::cout << "config.xml: You can only specify an icmp type if you have already specified the protocol as icmp." << std::endl;
					exit(1);
				}
				content->icmp_type = atoi(textNode->nodeValue().c_str());
			} else if (elementNode->nodeName() == XMLConstants::ICMP_CODE) {
				if (content->protocol != ICMP){
					std::cout << "config.xml: You can only specify an icmp code if you have already specified the protocol as icmp." << std::endl;
					exit(1);
				}
				content->icmp_code = atoi(textNode->nodeValue().c_str());
			} else if (elementNode->nodeName() == XMLConstants::POLICY_TAG) {
				PolicyFactory* factory = PolicyFactory::getInstance();
				content->policy = factory->parse(textNode->nodeValue());
			} else if (elementNode->nodeName() == XMLConstants::IN_INTERFACE_TAG) {
				std::string inIf = textNode->nodeValue();
				removeWhitespace(inIf);
				if(!networkLayout->hasInterface(inIf)){
					std::cout << "config.xml: You can only specify an in-interface that's specified in network_layout.xml" << std::endl;
					exit(1);
				}
				content->inInterface = inIf;
			} else if (elementNode->nodeName() == XMLConstants::OUT_INTERFACE_TAG) {
				std::string outIf = textNode->nodeValue();
				removeWhitespace(outIf);
				if(!networkLayout->hasInterface(outIf)){
					std::cout << "config.xml: You can only specify an out-interface that's specified in network_layout.xml" << std::endl;
					exit(1);
				}
				content->outInterface = outIf;
			} else if (elementNode->nodeName() == XMLConstants::DUMP_TAG) {
				std::string dumpFile = textNode->nodeValue();
				removeWhitespace(dumpFile);
				std::string path = _path + dumpFile;
				if(!fileExist(path)){
					std::cout << "config.xml: The specified dump file " << textNode->nodeValue() << " is not found in the config folder." << std::endl;
					exit(1);
				}
				content->dumpPath = std::pair<std::string, bool>(path, false);
			} else if (elementNode->nodeName() == XMLConstants::ASCII_DUMP_TAG) {
				std::string dumpFile = textNode->nodeValue();
				removeWhitespace(dumpFile);
				std::string path = _path + dumpFile;
				if(!fileExist(path)){
					std::cout << "config.xml: The specified ascii dump file " << textNode->nodeValue() << " is not found in the config folder." << std::endl;
					exit(1);
				}
				content->dumpPath = std::pair<std::string, bool>(path, true);
			} else if (elementNode->nodeName() ==  XMLConstants::NUM_PACKETS) {
				content->numPackets = atoi(textNode->nodeValue().c_str());
			}
			elementNode = elementNode->nextSibling();
		}

		if (content->dumpPath.first!="") {
			//traffic from dump file detected.
			if(content->inInterface == "" || content->outInterface == ""){
				std::cout << "config.xml: If you specify a " << (content->dumpPath.second?XMLConstants::ASCII_DUMP_TAG:XMLConstants::DUMP_TAG) << ", then you must also specify " << XMLConstants::IN_INTERFACE_TAG << "and " << XMLConstants::OUT_INTERFACE_TAG << "." << std::endl;
				exit(1);
			}
			//check existence of in- and out-interfaces.
			if (!networkLayout->hasInterface(content->inInterface) || !networkLayout->hasInterface(content->outInterface)){
				std::cout << "config.xml: The specified default in- and out-interfaces " << content->inInterface << " and " << content->outInterface << " do not exist within network_layout.xml." << std::endl;
				exit(1);
			}
			checkDump(content->dumpPath, content->inInterface, content->outInterface, networkLayout);
		} else {
			//src ip and dst ip must be set
			if ((content->sourceIP.toString() == "0.0.0.0") || (content->destIP.toString()=="0.0.0.0")){
				std::cout << "config.xml: No source or destination IP address specified. Otherwise you have to specify a dump file." << std::endl;
				exit(1);
			}
			//No dump file specified
			if (networkLayout->isInternal(content->sourceIP.toString(), content->destIP.toString())){
				//src and dst if must be set
				if((content->inInterface=="") || (content->outInterface=="")){
					std::cout << "config.xml: INTERN: Based on the specified ip addresses " << content->sourceIP.toString() << " and "<< content->destIP.toString() << ", the packet is classified as internal firewall traffic. You must specify the in- and out-interfaces properly." << std::endl;
					exit(1);
				}
				//check existence of in- and out-interfaces.
				if (!networkLayout->hasInterface(content->inInterface) || !networkLayout->hasInterface(content->outInterface)){
					std::cout << "config.xml: INTERN: The specified in- and out-interfaces " << content->inInterface << " and " << content->outInterface << " do not exist within network_layout.xml." << std::endl;
					exit(1);
				}
				//check source and destination
				if(content->sourceIP.toString() != networkLayout->getIpAddress(content->outInterface)) {
					std::cout << "config.xml: INTERN: Source ip address " << content->sourceIP.toString() << " does not correspond to the specified out-interface " << content->outInterface << " (" << networkLayout->getIpAddress(content->outInterface)<< ")" << std::endl;
					exit(1);
				}
				if (content->destIP.toString() != networkLayout->getIpAddress(content->inInterface)){
					std::cout << "config.xml: INTERN: Destination ip address " << content->destIP.toString() << " does not correspond to the specified in-interface " << content->inInterface << " (" << networkLayout->getIpAddress(content->inInterface)<< ")" << std::endl;
					exit(1);
				}
			} else if (networkLayout->isOutput(content->sourceIP.toString())){
				//src if must be set
				if(content->outInterface == ""){
					std::cout << "config.xml: OUTPUT: Based on the specified ip addresses " << content->sourceIP.toString() << " and "<< content->destIP.toString() << ", the packet is classified as firewall output traffic. You must specify the out-interface properly." << std::endl;
					exit(1);
				}
				//check existence of the out-interface.
				if (!networkLayout->hasInterface(content->outInterface)){
					std::cout << "config.xml: OUTPUT: The specified out-interface " << content->outInterface << " does not exist within network_layout.xml." << std::endl;
					exit(1);
				}
				//check source
				if(content->sourceIP.toString() != networkLayout->getIpAddress(content->outInterface)){
					std::cout << "config.xml: OUTPUT: Source ip address " << content->sourceIP.toString() << " does not correspond to the specified out-interface " << content->outInterface << " (" << networkLayout->getIpAddress(content->outInterface)<< ")" << std::endl;
					exit(1);
				}
			} else if (networkLayout->isInput(content->destIP.toString())) {
				//dst if must be set
				if(content->inInterface == ""){
					std::cout << "config.xml: INPUT: Based on the specified ip addresses " << content->sourceIP.toString() << " and "<< content->destIP.toString() << ", the packet is classified as firewall input traffic. You must specify the in-interface properly." << std::endl;
					exit(1);
				}
				//check existence of the in-interface.
				if (!networkLayout->hasInterface(content->inInterface)){
					std::cout << "config.xml: INPUT: The specified in-interface " << content->inInterface << " does not exist within network_layout.xml." << std::endl;
					exit(1);
				}
				//check destination
				if(content->destIP.toString() != networkLayout->getIpAddress(content->inInterface)){
					std::cout << "config.xml: INPUT: Destination ip address " << content->destIP.toString() << " does not correspond to the specified in-interface " << content->inInterface << " (" << networkLayout->getIpAddress(content->inInterface)<< ")" << std::endl;
					exit(1);
				}
			} else {
				//forward
				//src and dst if must be set
				if((content->inInterface == "") || (content->outInterface == "")){
					std::cout << "config.xml: FORWARD: Based on the specified ip addresses " << content->sourceIP.toString() << " and "<< content->destIP.toString() << ", the packet is classified as forward firewall traffic. You must specify the in- and out-interfaces properly." << std::endl;
					exit(1);
				}
				//check existence of in- and out-interfaces.
				if (!networkLayout->hasInterface(content->inInterface) || !networkLayout->hasInterface(content->outInterface)){
					std::cout << "config.xml: FORWARD: The specified in- and out-interfaces " << content->inInterface << " and " << content->outInterface << " do not exist within network_layout.xml." << std::endl;
					exit(1);
				}
			}
		}
		_content.push_back(content);

		trafficNode = trafficNode->nextSibling();
	}

	return numTrafficNodes;
}

void ConfigParser::printClickTraffic(std::ostream& ostream, NetworkLayout* networkLayout){
	int counter = 0;
	for(std::vector<ConfigContent*>::iterator it = _content.begin(); it != _content.end(); it++){
		ostream << "//Traffic Block" << std::endl;
		if ((*it)->dumpPath.first != "") {
			ostream << ((*it)->dumpPath.second?"FromIPSummaryDump":"FromDump") << "(\"" << (*it)->dumpPath.first
					<< "\", STOP true)" << std::endl;
			ostream << "	-> EnsureEther(0x0800, " << networkLayout->getMacAddress((*it)->outInterface) << ", "
					<< networkLayout->getMacAddress((*it)->inInterface) << ")" << std::endl;
			ostream << "	-> Paint(COLOR " << (*it)->policy << ")" << std::endl;
			ostream << "	-> [0]copy;" << std::endl << std::endl;
		} else {
			if ((*it)->protocol == TCP) {
				int flags = 0;
				for(std::vector<Flags>::iterator it2 = (*it)->tcp_flags.begin(); it2 != (*it)->tcp_flags.end(); it2++){
					flags += *it2;
				}
				counter++;
				ostream << "Script(write tcp" << counter << ".send " << (*it)->sourceIP.toString() << " "
								<< (*it)->sourcePort << " " << (*it)->destIP.toString() << " " << (*it)->destPort
								<< " 0 0 " << flags << " " << (*it)->numPackets << " true);" << std::endl;
				ostream << "tcp" << counter << " :: TCPIPSend" << std::endl;
			} else if ((*it)->protocol == UDP) {
				ostream << "InfiniteSource(LIMIT " << (*it)->numPackets << ", STOP true)" << std::endl;
				ostream	<< "	-> UDPIPEncap(" << (*it)->sourceIP.toString() << ", " << (*it)->sourcePort
					<< ", " << (*it)->destIP.toString() << ", " << (*it)->destPort << ")" << std::endl;

			} else if ((*it)->protocol == ICMP) {
				ostream << "InfiniteSource(LIMIT " << (*it)->numPackets << ", STOP true)" << std::endl;
				ostream << "	-> ICMPIPEncap(" << (*it)->sourceIP.toString() << ", " << (*it)->destIP.toString()
					<< ", " << (*it)->icmp_type;
				if ((*it)->icmp_code != -1) {
					ostream << ", " << (*it)->icmp_code;
				}
				ostream << ")" << std::endl;
			} else {
				assert(false); //unsupported atm
			}
			ostream << "	-> EtherEncap(0x0800, " << networkLayout->getMacAddress((*it)->outInterface) << ", "
					<< networkLayout->getMacAddress((*it)->inInterface) << ")" << std::endl;
			ostream << "	-> Paint(COLOR " << (*it)->policy << ")" << std::endl;
			ostream << "	-> [0]copy;" << std::endl << std::endl;
		}
	}
}

void clickHelperPrinter(std::ostream& clickScript, std::string name, NetworkLayout* networkLayout){
	std::map<std::string, std::string> mapping = networkLayout->getMacIPMappings();
	clickScript << "check_" << name << " :: IPClassifier(" << std::endl;
	for(std::map<std::string, std::string>::iterator it = mapping.begin(); it != mapping.end();) {
		clickScript << name << " " << it->second;
		if ((++it) != mapping.end()) {
			clickScript << ", ";
		}
	}
	clickScript << ")" << std::endl;

	int count = 0;
	for(std::map<std::string, std::string>::iterator it = mapping.begin(); it != mapping.end();it++) {
		clickScript << "check_" << name << "[" << count << "] -> " << name << (count+1) << " :: Classifier(0/";
		Poco::StringTokenizer macTokenizer(it->first, ":", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		for(int index = 0; index < macTokenizer.count(); index++){
			clickScript << macTokenizer[index];
		}
		clickScript << ", -)" << std::endl;
		clickScript << name << (count+1) << "[0] -> Discard;" << std::endl;
		clickScript << name << (count+1) << "[1] -> bogusCounter;" << std::endl;
		count++;
	}
	clickScript << std::endl;

}

void checkDump(std::pair<std::string, bool> dump, std::string inInterface, std::string outInterface, NetworkLayout* networkLayout) {
	if (dump.second)
		return;

	//Set up output
	std::ofstream clickScript;
	clickScript.open((SCRIPT_DIR + "check_dump.click").c_str());

	//Generate click script
	clickScript << "//Please don't alter the content of this file generated by FireSim" << std::endl;
	clickScript << "//authors: Nico Van Looy (2008-2009) & Jens De Wit (2008-2010)" << std::endl;
	clickScript << std::endl;

	clickScript << "DriverManager(pause, print >>bogus.txt bogusCounter.count, stop);" << std::endl;

	clickScript << "bogusCounter :: Counter -> ToDump(\"" << OUTPUT_PATH << "bogus.dump\");" << std::endl;
	clickScript << std::endl;

	clickHelperPrinter(clickScript, "src", networkLayout);
	clickHelperPrinter(clickScript, "dst", networkLayout);

	std::map<std::string, std::string> mapping = networkLayout->getMacIPMappings();
	clickScript << "check_src_and_dst :: IPClassifier(" << std::endl;
	for(std::map<std::string, std::string>::iterator it = mapping.begin(); it != mapping.end();) {
		clickScript << "src " << it->second << "/32";
		if ((++it) != mapping.end()) {
			clickScript << ", ";
		}
	}
	clickScript << ")" << std::endl;

	int count = 0;
	for(std::map<std::string, std::string>::iterator it = mapping.begin(); it != mapping.end();it++) {
		clickScript << "check_src_and_dst[" << count << "] -> src_and_dst" << (count+1) << " :: Classifier(0/";
		Poco::StringTokenizer macTokenizer(it->first, ":", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		for(int index = 0; index < macTokenizer.count(); index++){
			clickScript << macTokenizer[index];
		}
		clickScript << ", -)" << std::endl;
		clickScript << "src_and_dst" << (count+1) << "[0] -> check_dst;" << std::endl;
		clickScript << "src_and_dst" << (count+1) << "[1] -> bogusCounter;" << std::endl;
		count++;
	}
	clickScript << std::endl;

	clickScript << "check_src_and_dst_mac :: Classifier(";
	for(std::map<std::string, std::string>::iterator it = mapping.begin(); it != mapping.end();) {
		clickScript << "!0/";
		Poco::StringTokenizer macTokenizer(it->first, ":", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		for(int index = 0; index < macTokenizer.count(); index++){
			clickScript << macTokenizer[index];
		}
		if ((++it) != mapping.end()) {
			clickScript << " ";
		}
	}
	clickScript << ", -)" << std::endl;
	clickScript << std::endl;

	clickScript << "check_src_and_dst_mac[0] -> bogusCounter;" << std::endl;
	clickScript << "check_src_and_dst_mac[1] -> dstMac1 :: Classifier(";
	for(std::map<std::string, std::string>::iterator it = mapping.begin(); it != mapping.end();) {
		clickScript << "!6/";
		Poco::StringTokenizer macTokenizer(it->first, ":", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		for(int index = 0; index < macTokenizer.count(); index++){
			clickScript << macTokenizer[index];
		}
		if ((++it) != mapping.end()) {
			clickScript << " ";
		}
	}
	clickScript << ", -);" << std::endl;
	clickScript << "dstMac1[0] -> bogusCounter;" << std::endl;
	clickScript << "dstMac1[1] -> Discard;" << std::endl;

	clickScript << "//Intern - input - forward - output iptables switch" << std::endl;
	networkLayout->printStartChainSwitch(clickScript, "IPTABLES");
	clickScript << "IPTABLES[0] -> check_src_and_dst; //Internal traffic" << std::endl; //Internal Traffic
	clickScript << "IPTABLES[1] -> check_src; //Output Chain" << std::endl; //Output Chain
	clickScript << "IPTABLES[2] -> check_dst; //Input Chain" << std::endl; //Input Chain
	clickScript << "IPTABLES[3] -> check_src_and_dst_mac; //Forward Chain" << std::endl; //Forward Chain
	clickScript << std::endl;

	clickScript << "inputDump :: FromDump(\"" << dump.first << "\", STOP true)" << std::endl;
	clickScript << "	-> EnsureEther(0x0800, " << networkLayout->getMacAddress(outInterface) << ", " << networkLayout->getMacAddress(inInterface) << ")" << std::endl;
	clickScript << "	-> MarkIPHeader(14)" << std::endl;
	clickScript << "	-> IPTABLES;" << std::endl;

	clickScript.close();

	//do simulation
	std::string shellCommand = "click " + SCRIPT_DIR + "check_dump.click";
	if (system(shellCommand.c_str()) == -1) {
		Logger::get("ConsoleLogger").fatal("shell command '" + shellCommand + "' has failed to execute. Aborting...");
		exit(1);
	}

	//analyze result
	std::string line;
	std::ifstream file("bogus.txt");
	if (file.is_open()) {
		getline(file, line);
		if(stringToInt(line) != 0) {
			Logger::get("ConsoleLogger").fatal("config.xml: Check out the MAC addresses and the IP addresses of the packets in bogus.dump in output folder. They must be conform to the interfaces of the real firewall (see network_layout.xml in config folder).");
			exit(1);
		} else {
			//Remove bogus.dump in output folder
			removeFile("bogus.dump", OUTPUT_PATH);
		}
		file.close();
		removeFile("bogus.txt", "");
	} else {
		Logger::get("ConsoleLogger").fatal("Failed to open bogus.txt created by the click script check_dump.click");
		exit(1);
	}
}

bool fileExist(std::string path){
	//shorewall.compiled
	std::ifstream check;
	check.open(path.c_str());
	return check.is_open();
	check.close();
}
