#include "ofxMadOscQuery.h"

ofxMadOscQuery::ofxMadOscQuery(){}
ofxMadOscQuery::~ofxMadOscQuery(){
	for(auto & parameter : parameterMap){
		ofRemoveListener(parameter.second.oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
	}
}

void ofxMadOscQuery::setup(string ip, int sendPort, int receivePort){
	this->ip = ip;
	this->sendPort = sendPort;
	this->receivePort = receivePort;
	this->receiveAddress = "http://"+ip+":"+ofToString(8010);
	oscSender.setup(ip, sendPort);
	oscReceiver.setup(receivePort);
}

//--------------------------------------------------------------
ofJson ofxMadOscQuery::receive(){
	ofHttpResponse resp = ofLoadURL(receiveAddress);
	if(resp.data.size() == 0){
		ofLog(OF_LOG_FATAL_ERROR) << "MadMapper not open!" << endl;
		return nullptr;
	}
	ofJson response;
	std::stringstream ssJSON;
	ssJSON << resp.data;
	ssJSON >> response;
	return response;
}

//--------------------------------------------------------------
void ofxMadOscQuery::setupMadParameterFromJson(MadParameter & newParameter, ofJson jsonParameterValues){
	newParameter.setOscAddress(jsonParameterValues["FULL_PATH"].get<std::string>());
	newParameter.setName(jsonParameterValues["DESCRIPTION"]);
	if(! jsonParameterValues["RANGE"].is_null() ){
		newParameter.setMin(jsonParameterValues["RANGE"].at(0)["MIN"]);
		newParameter.setMax(jsonParameterValues["RANGE"].at(0)["MAX"]);
	}
	newParameter.set( jsonParameterValues["VALUE"].at(0));
}

//--------------------------------------------------------------
void ofxMadOscQuery::createCustomPage(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, std::string fileName){
	ofJson json = ofLoadJson(fileName);
	for(auto& page : json["pages"]){
		std::string name = page["name"];
		MadParameterPage customPage = MadParameterPage(name, midiDevice);
		
		// Find matching surfaces
		for(auto& element : page["surfaces"]){
			addParameterToCustomPage(element, "surfaces", &customPage);
		}
		// Find matching fixtures
		for(auto& element : page["fixtures"]){
			addParameterToCustomPage(element, "fixtures", &customPage);
		}
		// Find matching medias
		for(auto& element : page["medias"]){
			addParameterToCustomPage(element, "medias", &customPage);
		}
		pages.push_front(customPage);
	}
}
//--------------------------------------------------------------
void ofxMadOscQuery::addParameterToCustomPage(ofJson element, std::string type, MadParameterPage* customPage){
	std::string elementName = ofToString(element).substr(1, ofToString(element).size() - 2);
	std::string typeName = "/" + type +"/";
	
	for(auto& surfaceParam : parameterMap){
		std::string paramName = ofToString(surfaceParam.first);
		if(elementName == "*"){
			// WILDCARD - take all matching element
			if((surfaceParam.first.rfind(typeName, 0) == 0) && !(surfaceParam.first.rfind(typeName + "selected", 0) == 0)){
				(*customPage).addParameter(&surfaceParam.second);
			}
		}else if(elementName == paramName){
			// only take the matching
			(*customPage).addParameter(&surfaceParam.second);
		}else if(matchesGroupWildcard(paramName, elementName)){
			(*customPage).addParameter(&surfaceParam.second);
		}else{
			auto parsedName = elementName.substr(2, ofToString(element).size());
			// split by "/"
			std::vector<std::string> seglist;
			std::stringstream ss(paramName);
			std::string segment;
			while(std::getline(ss, segment, '/')){
				seglist.push_back(segment);
			}
			if(seglist[3] == parsedName && seglist[2] != "selected"){
				(*customPage).addParameter(&surfaceParam.second);
			}
		}
	}
}
//--------------------------------------------------------------
bool ofxMadOscQuery::matchesGroupWildcard(std::string paramName, std::string elementName){
	if(paramName.find("Group") == std::string::npos){
		return false;
	}
	paramName = paramName.substr(1, ofToString(paramName).size()); // remove start and end
	std::stringstream ss(paramName);
	std::string segment;
	std::vector<std::string> segList;
	while(std::getline(ss, segment, '/')){
		segList.push_back(segment);
	}

	for(int i = segList.size(); i > 1; i--){
		auto segment = segList[i];

		if(elementName.find(segment) != std::string::npos && // if segment found
		   segment != "Group" &&
		   segment != ""){
			return true;
		}
	}
	
	return false;
}

//--------------------------------------------------------------
void ofxMadOscQuery::createSubPages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
	auto keyTypes = {"surfaces", "medias", "fixtures"};
	for(auto & keyType : keyTypes){
		for(auto & element : json["CONTENTS"][keyType]["CONTENTS"]){
			// exceptions not to add
			
			// Skip fixed descriptions
			auto skipDescriptions = {"Next", "Per Type Selection", "Previous", "Select","Select By Name","Selected", "selected"}; // Descriptions for surfaces, medias & fixtures
			for(auto & skipDescription : skipDescriptions){
				if(element["DESCRIPTION"] == skipDescription) continue;
			}
			
			auto keyword = element["DESCRIPTION"].get<std::string>();
			MadParameterPage page = MadParameterPage(keyword, midiDevice, true);
			
			// Add parameters
			for(auto& contents : element["CONTENTS"]){
				// exception not to add
				// Skip fixed descriptions
				auto skipDescriptions = {"Resolution", "Assign To Selected Surfaces", "Assign To All Surfaces", "Restart","Select", "selected"}; // Descriptions for surfaces, medias & fixtures
				for(auto & skipDescription : skipDescriptions){
					if(element["DESCRIPTION"] == skipDescription) continue;
				}
				
				if(contents["DESCRIPTION"] == "Opacity"){
					page.addParameter(createParameter(contents));
				}
				if(contents["DESCRIPTION"] == "Color"){
					for(auto& color : contents["CONTENTS"]){
						if(color["DESCRIPTION"] == "Red" || color["DESCRIPTION"] == "Green" || color["DESCRIPTION"] == "Blue"){
							page.addParameter(createParameter(color));
						}
					}
				}
				
				if(contents["DESCRIPTION"] == "fx"){
					for(auto& fx : contents["CONTENTS"]){
						if( fx["DESCRIPTION"] != "FX Type" && fx["TYPE"]=="f"){
							page.addParameter(createParameter(fx));
						}
					}
				}
				
				
				if(keyType == "medias" && contents["TYPE"] == "f"){
					page.addParameter(createParameter(contents));
				}
			}
			if(!page.isEmpty()){
				pages.push_back(page);
			}
		}
	}
}
//--------------------------------------------------------------
void ofxMadOscQuery::oscSendToMadMapper(ofxOscMessage &m){
	oscSender.sendMessage(m, false);
}

void ofxMadOscQuery::oscReceiveMessages(){
	while(oscReceiver.hasWaitingMessages()){
		ofxOscMessage m;
		oscReceiver.getNextMessage(m);
		ofLog() << "Received Messafe " << lastSelectedMedia << endl;
		
		if(m.getAddress() == "/medias/select_by_name"){
			lastSelectedMedia = m.getArgAsString(0);
			ofLog() << "Connected Media " << lastSelectedMedia << endl;
			
			ofNotifyEvent(mediaNameE, lastSelectedMedia, this);
		}
	}
}

//--------------------------------------------------------------
MadParameter* ofxMadOscQuery::createParameter(ofJson parameterValues){
	std::string key = parameterValues["FULL_PATH"];
	parameterMap[key] = MadParameter(parameterValues);
	auto val = &parameterMap.operator[](key);
	ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
	return val;
}
//--------------------------------------------------------------
MadParameter* ofxMadOscQuery::createParameter(ofJson parameterValues, std::string name){
	std::string key = parameterValues["FULL_PATH"];
	parameterMap[key] = MadParameter(parameterValues,name);
	auto val = &parameterMap.operator[](key);
	ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
	return val;
}

