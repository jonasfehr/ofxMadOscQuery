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
    
    this->madMapperJson = receive(); //ofLoadJson("rawExample.json"); //
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


void ofxMadOscQuery::updateValues(){
//    long time = ofGetElapsedTimeMillis();
    
    this->madMapperJson = receive();
//    cout << "receive "<< ofGetElapsedTimeMillis() - time << endl;
    for( auto & param:parameterMap){
//        long timeEach = ofGetElapsedTimeMillis();
        string path = param.second.getOscAddress();
        vector<string> pathSeg = ofSplitString(path, "/");
        ofJson json = madMapperJson;
        for(int i = 1; i<pathSeg.size(); i++){
            json = json["CONTENTS"][pathSeg[i]];
        }
        param.second.set(json["VALUE"].at(0));
//        cout << param.second.getName() << " " << ofGetElapsedTimeMillis() - timeEach << endl;
    }
//    cout << "end "<< ofGetElapsedTimeMillis() - time << endl;
}
//--------------------------------------------------------------
//void ofxMadOscQuery::createParameterMap(ofJson json){
//    
//    
// 
////    getParameterList(json["CONTENTS"]["surfaces"]["CONTENTS"], {"selected"});
//
////    getParameterList(json["CONTENTS"]["medias"]["CONTENTS"], {"next", "per_type_selection", "previous", "select", "select_by_name", "selected"});
//    
//    iterateContents(json["CONTENTS"]["surfaces"]);
//    cout << parameterMap.size() << endl;
//    
//}

//void ofxMadOscQuery::iterateContents(ofJson json){
//    if(json["TYPE"]=="f") createParameter(json);
//    // dig deeper
//    for (nlohmann::json::iterator it = json.begin(); it != json.end(); ++it) {
//        if(it.key() == "CONTENTS"){
//            for (nlohmann::json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
//                cout << it2.key() << endl;
//                iterateContents(it2.value());
//            }
//            cout << "________________________" << endl;
//        }
//    }
//}


void ofxMadOscQuery::iterateFind(ofJson &jsonReturn, ofJson json, string key, ofJson jsonSkipKeys){
    
    if(json["FULL_PATH"].is_null()) return;
    string path = json["FULL_PATH"];
    vector<string> pathSeg = ofSplitString(path, "/");
    vector<string> keySeg = ofSplitString(key, "/");
    
    bool isKeyCompatible = true;
    for(int i = 0; i < keySeg.size() || i < pathSeg.size(); i++){
        if(keySeg[i] == pathSeg[i]){
        }else if( keySeg[i] == "*"){
            for(auto & skipKey : jsonSkipKeys){
                if(pathSeg[i] == skipKey.get<std::string>()){
                    isKeyCompatible = false;
                }
            }
        } else {
            isKeyCompatible = false;
        }
        
    }
    
    if(json["TYPE"] =="f" && isKeyCompatible){
        jsonReturn = json;
    }
    else{
        // dig deeper
        for (nlohmann::json::iterator it = json.begin(); it != json.end(); ++it) {
            if(it.key() == "CONTENTS"){
                for (nlohmann::json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
                    //                cout << it2.key() << endl;
                     iterateFind(jsonReturn, it2.value(), key, jsonSkipKeys);
                }
                //            cout << "________________________" << endl;
            }
        }
    }
}

void ofxMadOscQuery::iterateFind(ofJson json, string key, MadParameterPage* customPage, ofJson jsonSkipKeys){
    if(json["FULL_PATH"].is_null()) return;
    string path = json["FULL_PATH"];
//    if(path == "/medias/shpereMap_7.fs/audio_level" && key == "/medias/shpereMap_7.fs/*"){
//        cout << "x" << endl;
//    }
    vector<string> pathSeg = ofSplitString(path, "/");
    vector<string> keySeg = ofSplitString(key, "/");

    bool isKeyCompatible = true;
    int j = 0;
    int i = 0;
    while(j < keySeg.size() && i < pathSeg.size()){
        if(keySeg[j] == pathSeg[i]){
            j++;
            i++;
        }else if( keySeg[j] == "*"){
            for(auto & skipKey : jsonSkipKeys){
                for(int n = j; n <= i; n++){
                    //                    if(pathSeg[n] == "audio_level"){
                    //                        cout << pathSeg[n] << endl;
                    //                    }
                    if(pathSeg[n] == skipKey.get<std::string>()){
                        isKeyCompatible = false;
                    }
                }
            }

            if(keySeg.size()-1 != j){ // make sure there is more to come
                std::size_t foundPos = path.find("/"+keySeg[j+1]);
                if (foundPos!=std::string::npos){
                    int endPos = foundPos+keySeg[j+1].size()+1;
                    if(path[endPos] == '/' || endPos == path.size()){  // check if at the end or has a "/" to follow
                        while(pathSeg[i] != keySeg[j+1]){
                            i++;
                        }
                    } else {
                        isKeyCompatible = false;
                    }
                }
            } else {
                if( pathSeg.size() == keySeg.size() && isKeyCompatible) {
                    j = keySeg.size();
                }
            }
            j++;
        } else {
            isKeyCompatible = false;
            j++;
            i++;
        }

    }
    if(json["TYPE"]=="f" && isKeyCompatible){
        (*customPage).addParameter(createParameter(json));
    }


//    if(keySeg.back()=="opacity" && isKeyCompatible){
//        ofJson jsonSubpages = ofLoadJson("subpages.json");
//        std::string name = pathSeg[pathSeg.size()-2];
//        MadParameterPage customSubpage = MadParameterPage(name, midiDevice, true);
//        // Find matching surfaces
//        for(auto& element : jsonSubpages["opacity"]["elements"]){
//            string newKey = key;
//            ofStringReplace(newKey, "*", name);
//            ofStringReplace(newKey, "/opacity", element);
//            cout << newKey << endl;
//            cout << customSubpage.getParameters()->size() << endl;
//            iterateFind(json["CONTENTS"], newKey, &customSubpage, jsonSubpages["opacity"]["skipKeys"], midiDevice, subPages);
//        }
//        subPages.push_back(customSubpage);
//    }



    // dig deeper
    for (nlohmann::json::iterator it = json.begin(); it != json.end(); ++it) {
        if(it.key() == "CONTENTS"){
            for (nlohmann::json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
//                cout << it2.key() << endl;
                iterateFind(it2.value(), key, customPage, jsonSkipKeys);
            }
//            cout << "________________________" << endl;
        }
    }
}


//void ofxMadOscQuery::getParameterList(ofJson json, vector<string> skipKeys){
//
//    for (nlohmann::json::iterator it = json.begin(); it != json.end(); ++it) {
//        bool doSkip = false;
//        for(auto & skipKey : skipKeys){
//            if(it.key() == skipKey) doSkip = true;
//        }
//        if(!doSkip){
//            cout << it.key() << endl;
//        }
//    }
//}

map<string,ofJson> ofxMadOscQuery::getContentMap(ofJson json, string key, vector<string> skipKeys){
    
    map<string,ofJson> contentMap;
    vector<string> keySeg = ofSplitString(key, "/");
    
    
//    for(int level = 0; level < keySeg.size(); levle++)
    auto container = json["CONTENTS"][keySeg[1]]["CONTENTS"];
    for (nlohmann::json::iterator it = container.begin(); it != container.end(); ++it) {

        bool isKeyCompatible = true;
        if( keySeg[2] == "*"){
            for(auto & skipKey : skipKeys){
                if(it.key() == skipKey){
                    isKeyCompatible = false;
                }
            }
        } else {
            isKeyCompatible = false;
        }
    
        if(isKeyCompatible){
            contentMap[it.key()] = it.value();
        }
        

        // std::size_t found = path.find(keyWord);
        //if (found!=std::string::npos)
        
    }
    
    return contentMap;
}

void ofxMadOscQuery::getConnectedMediaName(string * mediaName, ofJson json, string key, ofJson jsonSkipKeys){
    
    if(json["FULL_PATH"].is_null()) return;
    string path = json["FULL_PATH"];
    vector<string> pathSeg = ofSplitString(path, "/");
    vector<string> keySeg = ofSplitString(key, "/");
    
    bool isKeyCompatible = true;
    int j = 0;
    int i = 0;
    while(j < keySeg.size() && i < pathSeg.size()){
        if(keySeg[j] == pathSeg[i]){
            j++;
            i++;
        }else if( keySeg[j] == "*"){
            
            if(keySeg.size()-1 != j){ // make sure there is more to come
                std::size_t foundPos = path.find("/"+keySeg[j+1]);
                if (foundPos!=std::string::npos){
                    int endPos = foundPos+keySeg[j+1].size()+1;
                    if(path[endPos] == '/' || endPos == path.size()){  // check if at the end or has a "/" to follow
                        while(pathSeg[i] != keySeg[j+1]){
                            i++;
                        }
                    } else {
                        isKeyCompatible = false;
                    }
                }
            } else {
                if( pathSeg.size() == keySeg.size() && isKeyCompatible) {
                    j = keySeg.size();
                }
            }
            
            for(auto & skipKey : jsonSkipKeys){
                for(int n = j; n < i; n++){
                    if(pathSeg[n] == skipKey.get<std::string>()){
                        isKeyCompatible = false;
                    }
                }
            }
            j++;
        } else {
            isKeyCompatible = false;
            j++;
            i++;
        }
        
    }
    if(json["TYPE"]=="s" && isKeyCompatible){
        *mediaName = json["VALUE"][0].get<std::string>();
        ofStringReplace(*mediaName, " ", "_");
//        cout << mediaName << endl;
    }
    
    
    // dig deeper
    for (nlohmann::json::iterator it = json.begin(); it != json.end(); ++it) {
        if(it.key() == "CONTENTS"){
            for (nlohmann::json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
                //                cout << it2.key() << endl;
                 getConnectedMediaName(mediaName, it2.value(), key, jsonSkipKeys);
            }
            //            cout << "________________________" << endl;
        }
    }
}


//--------------------------------------------------------------
void ofxMadOscQuery::createCustomPages(ofxMidiDevice* midiDevice, ofJson jsonPages, ofJson madMapperJson){
    for(auto& page : jsonPages["pages"]){
        std::string name = page["name"];
        MadParameterPage customPage = MadParameterPage(name, midiDevice);
        
        // Find matching surfaces
        for(auto& element : page["elements"]){
            iterateFind(madMapperJson, element, &customPage, page["skipKeys"]);
//            ofJson jsonParam;
//            iterateFind(jsonParam, madMapperJson, element, page["skipKeys"]);
//            if(!jsonParam.is_null()) customPage.addParameter(createParameter(jsonParam));

        }

        pages.push_back(customPage);
    }
    
    // create the necessary subpages
    ofJson jsonSubpages = ofLoadJson("subpages.json");

    for( auto& param : parameterMap){
        if(param.second.isMaster){
            std::string name = param.second.parentName;
            
            // Opacity Pages
            MadParameterPage customSubpage = MadParameterPage(name, midiDevice, true);
//            // Find matching surfaces
            for(auto& element : jsonSubpages["opacity"]["elements"]){
                string newKey = "*/"+name+element.get<std::string>();
//                ofStringReplace(newKey, "*", name);
//                ofStringReplace(newKey, "/opacity", element);
//                cout << newKey << endl;
//                cout << customSubpage.getParameters()->size() << endl;
                iterateFind(madMapperJson, newKey, &customSubpage, jsonSubpages["opacity"]["skipKeys"]);
            }
            subPages.push_back(customSubpage);
            
            // connected Media
            string newKey = "*/"+name+"/visual/name";
            string mediaName;
            getConnectedMediaName(&mediaName, madMapperJson, newKey,  jsonSubpages["medias"]["skipKeys"] );
        
            if(mediaName.size()>0) {
                if(mediaName != "4x4.png"){
                    MadParameterPage customMediaSubpage = MadParameterPage(mediaName, midiDevice, true);
                    param.second.setConnectedMediaName( mediaName );

                    for(auto& element : jsonSubpages["medias"]["elements"]){
                        string mediaKey = "/medias/"+mediaName+element.get<std::string>();
                        iterateFind(madMapperJson, mediaKey, &customMediaSubpage, jsonSubpages["medias"]["skipKeys"]);
                    }
                    subPages.push_back(customMediaSubpage);
                }
            }
        }
    }
    
    for( auto& param : parameterMap){
        if(param.second.isMaster){
            std::string name = param.second.parentName;
            MadParameterPage customSubpage = MadParameterPage(name, midiDevice, true);
            //            // Find matching surfaces
            for(auto& element : jsonSubpages["media"]["elements"]){
                string newKey = "*/"+name+element.get<std::string>();
                //                ofStringReplace(newKey, "*", name);
                //                ofStringReplace(newKey, "/opacity", element);
                //                cout << newKey << endl;
                //                cout << customSubpage.getParameters()->size() << endl;
                iterateFind(madMapperJson, newKey, &customSubpage, jsonSubpages["opacity"]["skipKeys"]);
            }
            subPages.push_back(customSubpage);
        }
    }

    
    
    
//    for(auto& subpage : jsonPages["subpages"]){
//        map<string,ofJson> contentMap = getContentMap(madMapperJson, subpage["key"], subpage["skipKeys"]);
//        for(auto & content : contentMap){
//            std::string name = content.first;
//            MadParameterPage customSubpage = MadParameterPage(name, midiDevice, true);
//            // Find matching surfaces
//            for(auto& element : subpage["elements"]){
//
//                iterateFind(content.second, element, &customSubpage, subpage["skipKeys"]);
//            }
//            subPages.push_back(customSubpage);
//        }
//    }


    
}

//--------------------------------------------------------------
void ofxMadOscQuery::createCustomPage(std::list<MadParameterPage> &pages, ofxMidiDevice* midiDevice, ofJson json){
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
			// Find matching parameters from groups
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
	// returns true if paramName matches a groups and has a wildcard "*"
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

	for(int i = segList.size()-1; i > 1; i--){
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
            bool shouldSkip = false;
            for(auto & skipDescription : skipDescriptions){
                if(element["DESCRIPTION"] == skipDescription){
                    shouldSkip = true;
                    break;
                }
            }
            if(shouldSkip) continue;
            
            auto keyword = element["DESCRIPTION"].get<std::string>();
            MadParameterPage page = MadParameterPage(keyword, midiDevice, true);
            
            setupPageFromJson(pages, page, midiDevice, element, keyType);
            
            // Add parameters
            
            if(!page.isEmpty()){
                pages.push_back(page);
            }
        }
    }
}
bool ofxMadOscQuery::setupPageFromJson(std::list<MadParameterPage> &pages, MadParameterPage & page, ofxMidiDevice* midiDevice, ofJson element, string keyType){
    
//    bool bIsGroup = false;
//    // detect Groups
//    if (keyType=="surfaces" && element["CONTENTS"]["invert_mask"].is_null()){
//        cout << "Detected as Group" << endl;
//        bIsGroup = true;
//
//
//    }
//    else if(keyType=="fixtures" && element["CONTENTS"].size()>9){
//        cout << "Detected as Group" << endl;
//        bIsGroup = true;
//    }
//    page.setIsGroup(bIsGroup);
//
//    if(bIsGroup && element["DESCRIPTION"].is_string()){
//        auto keyword = element["DESCRIPTION"].get<std::string>()+"_SubPage";
//        string searchString = "/surfaces/"+keyword+"/*/opacity";
//
//        cout << searchString << endl;
//
//
//        ofJson customJson = "{ \"pages\": [{\"name\": "+keyword+", \"surfaces\": ["+searchString+"]}]}";
//
//        //createCustomPage(pages, midiDevice, customJson);
//    }
    
    for(auto& contents : element["CONTENTS"]){
        if(contents["DESCRIPTION"].is_null() || !contents["DESCRIPTION"].is_string()) continue;
        // exception not to add
        // Skip fixed descriptions
        
        auto skipDescriptions = {"Resolution", "Assign To Selected Surfaces", "Assign To All Surfaces", "Restart","Select", "selected"}; // Descriptions for surfaces, medias & fixtures
        bool shouldSkip = false;
        for(auto & skipDescription : skipDescriptions){
            if(element["CONTENTS"]["DESCRIPTION"] == skipDescription){
                shouldSkip = true;
                return;
            }
        }
        if(shouldSkip) continue;
        
        if(contents["DESCRIPTION"] == "Opacity"){
            MadParameter * newOpacityParameter = createParameter(contents);
            page.addParameter(newOpacityParameter);
            
            // detect if it is a groupe
            bool bIsGroup = false;
            // detect Groups
            if (keyType=="surfaces" && element["CONTENTS"]["invert_mask"].is_null()){
                cout << "Detected as Group" << endl;
                bIsGroup = true;
            }else if(keyType=="fixtures" && element["CONTENTS"].size()>10){
                cout << "Detected as Group" << endl;
                bIsGroup = true;
            }else return;
            
            newOpacityParameter->setIsGroup(bIsGroup);
            
            auto groupName = element["DESCRIPTION"].get<std::string>();
            MadParameterPage subPage = MadParameterPage(groupName, midiDevice, true);
            setupPageFromJson(pages, subPage, midiDevice, contents["CONTENTS"], keyType);
            
            // create opacity subpage for all surfaces/fixtures in the group
                string searchString = groupName+"/*/opacity";
            
                
            auto customJson = ofJson::parse("{ \"pages\": [{\"name\": \""+groupName+"_SubPage\", \"surfaces\": [\""+searchString+"\"]}]}");
            
            cout << customJson << endl;
                createCustomPage(pages, midiDevice, customJson);
            
            
                // create pages for elements in group
                
//                // Skip fixed descriptions
//                auto skipDescriptionsForGroup = {"Blend Mode", "Color", "Input", "Opacity","Output", "Select", "Visible", "Visual", "Luminosity", "Response", "Sliders"}; // Descriptions for surfaces, medias & fixtures
//                bool shouldSkip = false;
//                for(auto & skipDescription : skipDescriptionsForGroup){
//                    if(contents["DESCRIPTION"] == skipDescription){
//                        shouldSkip=true;
//                        break;
//                    }
//                }
//                if(shouldSkip) continue;
            
                // create a page for the surfaces/fixtures in the group

                
//            }

            
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
}
//--------------------------------------------------------------
void ofxMadOscQuery::oscSendToMadMapper(ofxOscMessage &m){
	oscSender.sendMessage(m, false);
}

void ofxMadOscQuery::oscReceiveMessages(){
//    while(oscReceiver.hasWaitingMessages()){
//        ofxOscMessage m;
//        oscReceiver.getNextMessage(m);
//        ofLog() << "Received Messafe " << lastSelectedMedia << endl;
//
//        if(m.getAddress() == "/medias/select_by_name"){
//            lastSelectedMedia = m.getArgAsString(0);
//            ofLog() << "Connected Media " << lastSelectedMedia << endl;
//
//            ofNotifyEvent(mediaNameE, lastSelectedMedia, this);
//        }
//    }
}

//--------------------------------------------------------------
MadParameter* ofxMadOscQuery::createParameter(ofJson parameterValues){
	std::string key = parameterValues["FULL_PATH"];
	parameterMap[key] = MadParameter(parameterValues);
	auto val = &parameterMap.operator[](key);
	ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
	return val;
}
////--------------------------------------------------------------
//MadParameter* ofxMadOscQuery::createParameter(ofJson parameterValues, std::string name){
//    std::string key = parameterValues["FULL_PATH"];
//    parameterMap[key] = MadParameter(parameterValues,name);
//    auto val = &parameterMap.operator[](key);
//    ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
//    return val;
//}


