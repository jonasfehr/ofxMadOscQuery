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
void ofxMadOscQuery::createOpacityPages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
    // Create pages for opacity value for each surface
    std::string keyword = "SurfaceMixer";
    MadParameterPage page = MadParameterPage(keyword, midiDevice);
    for(auto & element : json["CONTENTS"]["surfaces"]["CONTENTS"]){
        if(element["DESCRIPTION"] == "selected"){
            continue; // skip this one
        }
        // Add parameters
        for(auto& contents : element["CONTENTS"]){
            if(contents["DESCRIPTION"] == "Opacity"){
                page.addParameter(createParameter(contents));
            }
        }
    }
    if(!page.isEmpty()){
        pages.push_back(page);
    }
}

//--------------------------------------------------------------
void ofxMadOscQuery::createSurfacePages(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
    std::string name = "surface";
    int idx = 0;
    for(auto & element : json["CONTENTS"]["surfaces"]["CONTENTS"]){
        if(element["DESCRIPTION"] == "selected"){
            continue; // skip this one
        }
        auto keyword = element["DESCRIPTION"].get<std::string>();//name + "_" + ofToString(idx);
        MadParameterPage page = MadParameterPage(keyword, midiDevice);
        
        // Add parameters
        for(auto& contents : element["CONTENTS"]){
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
        }
        if(!page.isEmpty()){
            pages.push_back(page);
            idx++;
        }
    }
}
//--------------------------------------------------------------
void ofxMadOscQuery::createMediaPages(std::map<string, MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
    for(auto & element : json["CONTENTS"]["medias"]["CONTENTS"]){
        // exceptions not to add
        if(element["DESCRIPTION"] == "Next" || element["DESCRIPTION"] == "Per Type Selection" || element["DESCRIPTION"] == "Previous" || element["DESCRIPTION"] == "Select" || element["DESCRIPTION"] == "Select By Name" || element["DESCRIPTION"] == "Selected"){
            continue; // skip this one
        }
        
        auto keyword = element["DESCRIPTION"].get<std::string>();
        MadParameterPage page = MadParameterPage(keyword, midiDevice);
        
        // Add parameters
        for(auto& contents : element["CONTENTS"]){
            // exception not to add
            if(contents["DESCRIPTION"] == "Resolution" || contents["DESCRIPTION"] == "Assign To Selected Surfaces" || contents["DESCRIPTION"] == "Assign To All Surfaces" || contents["DESCRIPTION"] == "Restart" || contents["DESCRIPTION"] == "Select" || contents["DESCRIPTION"] == "mappingImage"){
                continue; // skip this one
            }
            
            if(contents["TYPE"] == "f"){
                page.addParameter(createParameter(contents));
            }
        }
        if(!page.isEmpty()){
            pages.insert(std::make_pair(keyword,page));
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
        
        if(m.getAddress() == "/medias/select_by_name"){
            lastSelectedMedia = m.getArgAsString(0);
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

