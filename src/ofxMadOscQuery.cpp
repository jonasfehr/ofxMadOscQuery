#include "ofxMadOscQuery.h"
#include "OscQueryWebSocketClient.h"

ofxMadOscQuery::ofxMadOscQuery() { }
ofxMadOscQuery::~ofxMadOscQuery() {
	for (auto & parameter : parameterMap) {
		ofRemoveListener(parameter.second.oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
	}
}

void ofxMadOscQuery::setup(string ip, int sendPort, int feedbackPort, int queryPort) {
	this->ip = ip;
	this->sendPort = sendPort;
	this->receivePort = feedbackPort;
	this->receiveAddress = "http://" + ip + ":" + ofToString(queryPort);
	oscSender.setup(ip, sendPort);
	oscReceiver.setup(feedbackPort);

	//    this->madMapperJson = receive(); // ofLoadJson("rawExample.json"); //
}

void ofxMadOscQuery::setup(string ip, int sendPort, int feedbackPort) {
	this->setup(ip, sendPort, feedbackPort, sendPort);
}
//--------------------------------------------------------------
ofJson ofxMadOscQuery::receive() {
	ofHttpResponse resp = ofLoadURL(receiveAddress);
	if (resp.data.size() == 0) {
		ofLogWarning("ofxMadOscQuery") << "MadMapper not open or OSCQuery endpoint unreachable: " << receiveAddress;
		return nullptr;
	}
	ofJson response;
	std::stringstream ssJSON;
	ssJSON << resp.data;
	ssJSON >> response;
	this->madMapperJson = response;
	return response;
}

void ofxMadOscQuery::updateValues() {
	this->madMapperJson = receive();
	for (auto & param : parameterMap) {
		string path = param.second.getOscAddress();
		vector<string> pathSeg = ofSplitString(path, "/");
		ofJson json = madMapperJson;
		for (int i = 1; i < (int)pathSeg.size(); i++) {
			json = json["CONTENTS"][pathSeg[i]];
		}
		if (!json.is_null()) {
			param.second.setFromRemoteRaw(json["VALUE"].at(0));
		}
	}
}
//--------------------------------------------------------------
// void ofxMadOscQuery::createParameterMap(ofJson json){
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

// void ofxMadOscQuery::iterateContents(ofJson json){
//     if(json["TYPE"]=="f") createParameter(json);
//     // dig deeper
//     for (nlohmann::json::iterator it = json.begin(); it != json.end(); ++it) {
//         if(it.key() == "CONTENTS"){
//             for (nlohmann::json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
//                 cout << it2.key() << endl;
//                 iterateContents(it2.value());
//             }
//             cout << "________________________" << endl;
//         }
//     }
// }

void ofxMadOscQuery::iterateFind(ofJson & jsonReturn, const ofJson & json, const string & key, const ofJson & jsonSkipKeys) {

	auto itFull = json.find("FULL_PATH");
	if (itFull == json.end() || !itFull->is_string()) return;
	string path = itFull->get<std::string>();
	vector<string> pathSeg = ofSplitString(path, "/");
	vector<string> keySeg = ofSplitString(key, "/");

	bool isKeyCompatible = true;
	for (size_t i = 0; i < keySeg.size() && i < pathSeg.size(); i++) {
		if (keySeg[i] == pathSeg[i]) {
		} else if (keySeg[i] == "*") {
			for (auto & skipKey : jsonSkipKeys) {
				if (pathSeg[i] == skipKey.get<std::string>()) {
					isKeyCompatible = false;
				}
			}
		} else {
			isKeyCompatible = false;
		}
	}

	auto itType = json.find("TYPE");
	if (itType != json.end() && itType->is_string() && (*itType == "f") && isKeyCompatible) {
		jsonReturn = json;
		return;
	}

	for (auto it = json.begin(); it != json.end(); ++it) {
		if (it.key() == "CONTENTS") {
			for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
				iterateFind(jsonReturn, it2.value(), key, jsonSkipKeys);
			}
		}
	}
}

void ofxMadOscQuery::iterateFind(const ofJson & json, const string & key, MadParameterPage * customPage, const ofJson & jsonSkipKeys) {
	auto itFull = json.find("FULL_PATH");
	if (itFull == json.end() || !itFull->is_string()) return;
	string path = itFull->get<std::string>();
	vector<string> pathSeg = ofSplitString(path, "/");
	vector<string> keySeg = ofSplitString(key, "/");

	bool isKeyCompatible = true;
	int j = 0;
	int i = 0;
	while (j < (int)keySeg.size() && i < (int)pathSeg.size()) {
		if (keySeg[j] == pathSeg[i]) {
			j++;
			i++;
		} else if (keySeg[j] == "*") {
			for (auto & skipKey : jsonSkipKeys) {
				for (int n = j; n <= i; n++) {
					if (pathSeg[n] == skipKey.get<std::string>()) {
						isKeyCompatible = false;
					}
				}
			}

			if ((int)keySeg.size() - 1 != j) {
				std::size_t foundPos = path.find("/" + keySeg[j + 1]);
				if (foundPos != std::string::npos) {
					int endPos = (int)foundPos + (int)keySeg[j + 1].size() + 1;
					if ((endPos == (int)path.size()) || path[endPos] == '/') {
						while (i < (int)pathSeg.size() && pathSeg[i] != keySeg[j + 1]) {
							i++;
						}
						if (i == j) isKeyCompatible = false;
					} else {
						isKeyCompatible = false;
					}
				}
			} else {
				if ((int)pathSeg.size() == (int)keySeg.size() && isKeyCompatible) {
					j = (int)keySeg.size();
				}
			}
			j++;
		} else {
			isKeyCompatible = false;
			break;
		}
	}

	auto itType = json.find("TYPE");
	if (itType != json.end() && itType->is_string() && (*itType == "f") && isKeyCompatible) {
		(*customPage).addParameter(createParameter(json));
	}

	for (auto it = json.begin(); it != json.end(); ++it) {
		if (it.key() == "CONTENTS") {
			for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
				iterateFind(it2.value(), key, customPage, jsonSkipKeys);
			}
		}
	}
}

map<string, ofJson> ofxMadOscQuery::getContentMap(const ofJson & json, const string & key, const vector<string> & skipKeys) {
	map<string, ofJson> contentMap;
	vector<string> keySeg = ofSplitString(key, "/");
	if (keySeg.size() < 2) return contentMap;

	auto itTop = json.find("CONTENTS");
	if (itTop == json.end() || !itTop->is_object()) return contentMap;

	auto itCat = itTop->find(keySeg[1]);
	if (itCat == itTop->end() || !itCat->is_object()) return contentMap;

	auto itInner = itCat->find("CONTENTS");
	if (itInner == itCat->end() || !itInner->is_object()) return contentMap;

	const auto & container = *itInner;
	for (auto it = container.begin(); it != container.end(); ++it) {
		bool isKeyCompatible = true;
		if (keySeg.size() > 2 && keySeg[2] == "*") {
			for (auto & skipKey : skipKeys) {
				if (it.key() == skipKey) {
					isKeyCompatible = false;
				}
			}
		} else {
			isKeyCompatible = false;
		}

		if (isKeyCompatible) {
			contentMap[it.key()] = it.value();
		}
	}

	return contentMap;
}

void ofxMadOscQuery::getConnectedMediaName(string * mediaName, const ofJson & json, const string & key, const ofJson & jsonSkipKeys) {
	auto itFull = json.find("FULL_PATH");
	if (itFull == json.end() || !itFull->is_string()) return;
	string path = itFull->get<std::string>();
	vector<string> pathSeg = ofSplitString(path, "/");
	vector<string> keySeg = ofSplitString(key, "/");

	bool isKeyCompatible = true;
	int j = 0;
	int i = 0;
	while (j < (int)keySeg.size() && i < (int)pathSeg.size()) {
		if (keySeg[j] == pathSeg[i]) {
			j++;
			i++;
		} else if (keySeg[j] == "*") {
			if ((int)keySeg.size() - 1 != j) {
				std::size_t foundPos = path.find("/" + keySeg[j + 1]);
				if (foundPos != std::string::npos) {
					int endPos = (int)foundPos + (int)keySeg[j + 1].size() + 1;
					if ((endPos == (int)path.size()) || path[endPos] == '/') {
						while (i < (int)pathSeg.size() && pathSeg[i] != keySeg[j + 1]) {
							i++;
						}
					} else {
						isKeyCompatible = false;
					}
				}
			} else {
				if ((int)pathSeg.size() == (int)keySeg.size() && isKeyCompatible) {
					j = (int)keySeg.size();
				}
			}

			for (auto & skipKey : jsonSkipKeys) {
				for (int n = j; n < i; n++) {
					if (pathSeg[n] == skipKey.get<std::string>()) {
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
	auto itType = json.find("TYPE");
	if (itType != json.end() && itType->is_string() && (*itType == "s") && isKeyCompatible) {
		auto itVal = json.find("VALUE");
		if (itVal != json.end() && itVal->is_array() && !itVal->empty() && (*itVal)[0].is_string()) {
			*mediaName = (*itVal)[0].get<std::string>();
			ofStringReplace(*mediaName, " ", "_");
		}
	}

	for (auto it = json.begin(); it != json.end(); ++it) {
		if (it.key() == "CONTENTS") {
			for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
				getConnectedMediaName(mediaName, it2.value(), key, jsonSkipKeys);
			}
		}
	}
}

void ofxMadOscQuery::createCustomPages(ofxMidiDevice * midiDevice, const ofJson & jsonPages, const ofJson & madMapperJson) {
	for (auto & page : jsonPages["pages"]) {
		std::string name = page["name"];
		MadParameterPage customPage = MadParameterPage(name, midiDevice);
		for (auto & element : page["elements"]) {
			iterateFind(madMapperJson, element, &customPage, page["skipKeys"]);
		}
		pages.push_back(customPage);
	}

	ofJson jsonSubpages = ofLoadJson("subpages.json");

	for (auto & param : parameterMap) {
		if (param.second.isMaster) {
			std::string name = param.second.parentName;

			MadParameterPage customSubpage = MadParameterPage(name, midiDevice, true);
			for (auto & element : jsonSubpages["opacity"]["elements"]) {
				string newKey = "*/" + name + element.get<std::string>();
				iterateFind(madMapperJson, newKey, &customSubpage, jsonSubpages["opacity"]["skipKeys"]);
			}
			subPages.push_back(customSubpage);

			for (auto & element : jsonSubpages["luminosity"]["elements"]) {
				string newKey = "*/" + name + element.get<std::string>();
				iterateFind(madMapperJson, newKey, &customSubpage, jsonSubpages["luminosity"]["skipKeys"]);
			}
			subPages.push_back(customSubpage);

			string newKey = "*/" + name + "/visual/name";
			string mediaName;
			getConnectedMediaName(&mediaName, madMapperJson, newKey, jsonSubpages["medias"]["skipKeys"]);

			if (!mediaName.empty() && mediaName != "4x4.png") {
				MadParameterPage customMediaSubpage = MadParameterPage(mediaName, midiDevice, true);
				param.second.setConnectedMediaName(mediaName);
				for (auto & element : jsonSubpages["medias"]["elements"]) {
					string mediaKey = "/medias/" + mediaName + element.get<std::string>();
					iterateFind(madMapperJson, mediaKey, &customMediaSubpage, jsonSubpages["medias"]["skipKeys"]);
				}
				subPages.push_back(customMediaSubpage);
			}
		}
	}

	for (auto & param : parameterMap) {
		if (param.second.isMaster) {
			std::string name = param.second.parentName;
			MadParameterPage customSubpage = MadParameterPage(name, midiDevice, true);
			for (auto & element : jsonSubpages["media"]["elements"]) {
				string newKey = "*/" + name + element.get<std::string>();
				iterateFind(madMapperJson, newKey, &customSubpage, jsonSubpages["opacity"]["skipKeys"]);
			}
			subPages.push_back(customSubpage);
		}
	}
}

void ofxMadOscQuery::createCustomPage(std::list<MadParameterPage> & pages, ofxMidiDevice * midiDevice, const ofJson & json) {
	for (auto & page : json["pages"]) {
		std::string name = page["name"];
		MadParameterPage customPage = MadParameterPage(name, midiDevice);
		for (auto & element : page["surfaces"]) {
			addParameterToCustomPage(element, "surfaces", &customPage);
		}
		for (auto & element : page["fixtures"]) {
			addParameterToCustomPage(element, "fixtures", &customPage);
		}
		for (auto & element : page["medias"]) {
			addParameterToCustomPage(element, "medias", &customPage);
		}
		for (auto & element : page["modules"]) {
			addParameterToCustomPage(element, "modules", &customPage);
		}
		pages.push_front(customPage);
	}
}

void ofxMadOscQuery::addParameterToCustomPage(const ofJson & element, const std::string & type, MadParameterPage * customPage) {
	std::string elementName;
	if (element.is_string())
		elementName = element.get<std::string>();
	else
		elementName = ofToString(element).substr(1, ofToString(element).size() - 2);
	std::string typeName = "/" + type + "/";

	for (auto & surfaceParam : parameterMap) {
		std::string paramName = ofToString(surfaceParam.first);
		if (elementName == "*") {
			if ((surfaceParam.first.rfind(typeName, 0) == 0) && !(surfaceParam.first.rfind(typeName + "selected", 0) == 0)) {
				(*customPage).addParameter(&surfaceParam.second);
			}
		} else if (elementName == paramName) {
			(*customPage).addParameter(&surfaceParam.second);
		} else if (matchesGroupWildcard(paramName, elementName)) {
			(*customPage).addParameter(&surfaceParam.second);
		} else {
			auto parsedName = elementName.substr(2, elementName.size() - 2);
			std::vector<std::string> seglist;
			std::stringstream ss(paramName);
			std::string segment;
			while (std::getline(ss, segment, '/')) {
				seglist.push_back(segment);
			}
			if (seglist.size() > 3 && seglist[3] == parsedName && seglist[2] != "selected") {
				(*customPage).addParameter(&surfaceParam.second);
			}
		}
	}
}

bool ofxMadOscQuery::matchesGroupWildcard(const std::string & paramNameIn, const std::string & elementName) {
	std::string paramName = paramNameIn;
	if (paramName.find("Group") == std::string::npos) {
		return false;
	}
	paramName = paramName.substr(1, ofToString(paramName).size());
	std::stringstream ss(paramName);
	std::string segment;
	std::vector<std::string> segList;
	while (std::getline(ss, segment, '/')) {
		segList.push_back(segment);
	}

	for (int i = (int)segList.size() - 1; i > 1; i--) {
		auto seg = segList[i];
		if (!seg.empty() && seg != "Group" && elementName.find(seg) != std::string::npos) {
			return true;
		}
	}
	return false;
}

void ofxMadOscQuery::createSubPages(std::list<MadParameterPage> & pages, ofxMidiDevice * midiDevice, const ofJson & json) {
	auto itTop = json.find("CONTENTS");
	if (itTop == json.end() || !itTop->is_object()) return;

	auto keyTypes = { "surfaces", "medias", "fixtures" };
	for (auto & keyType : keyTypes) {
		auto itType = itTop->find(keyType);
		if (itType == itTop->end() || !itType->is_object()) continue;
		auto itCont = itType->find("CONTENTS");
		if (itCont == itType->end() || !itCont->is_object()) continue;

		for (auto it = itCont->begin(); it != itCont->end(); ++it) {
			const auto & element = it.value();
			auto itDesc = element.find("DESCRIPTION");
			if (itDesc == element.end() || !itDesc->is_string()) continue;

			auto skipDescriptions = { "Next", "Per Type Selection", "Previous", "Select", "Select By Name", "Selected", "selected", "children" };
			bool shouldSkip = false;
			for (auto & skipDescription : skipDescriptions) {
				if (*itDesc == skipDescription) {
					shouldSkip = true;
					break;
				}
			}
			if (shouldSkip) continue;

			auto keyword = itDesc->get<std::string>();
			MadParameterPage page = MadParameterPage(keyword, midiDevice, true);
			setupPageFromJson(pages, page, midiDevice, element, keyType);
			if (!page.isEmpty()) {
				pages.push_back(page);
			}
		}
	}
}

void ofxMadOscQuery::setupPageFromJson(std::list<MadParameterPage> & pages, MadParameterPage & page, ofxMidiDevice * midiDevice, const ofJson & element, const string & keyType) {
	auto itCont = element.find("CONTENTS");
	if (itCont == element.end() || !itCont->is_object()) return;

	for (auto it = itCont->begin(); it != itCont->end(); ++it) {
		const auto & contents = it.value();
		auto itDesc = contents.find("DESCRIPTION");
		if (itDesc == contents.end() || !itDesc->is_string()) continue;

		auto skipDescriptions = { "Resolution", "Assign To Selected Surfaces", "Assign To All Surfaces", "Restart", "Select", "selected" };
		for (auto & skipDescription : skipDescriptions) {
			auto innerDesc = element["CONTENTS"].find("DESCRIPTION");
			if (innerDesc != element["CONTENTS"].end() && *innerDesc == skipDescription) {
				return;
			}
		}

		if (*itDesc == "Opacity") {
			MadParameter * newOpacityParameter = createParameter(contents);
			page.addParameter(newOpacityParameter);
			bool bIsGroup = false;
			if (keyType == "surfaces" && element["CONTENTS"].find("output") == element["CONTENTS"].end()) {
				bIsGroup = true;
			} else if (keyType == "fixtures" && element["CONTENTS"].size() > 10) {
				bIsGroup = true;
			} else
				return;
			newOpacityParameter->setIsGroup(bIsGroup);
			auto groupNameIt = element.find("DESCRIPTION");
			std::string groupName = groupNameIt != element.end() && groupNameIt->is_string() ? groupNameIt->get<std::string>() : "Group";
			MadParameterPage subPage = MadParameterPage(groupName, midiDevice, true);
			auto subContIt = contents.find("CONTENTS");
			if (subContIt != contents.end() && subContIt->is_object()) {
				setupPageFromJson(pages, subPage, midiDevice, *subContIt, keyType);
			}
			string searchString = groupName + "/*/opacity";
			auto customJson = ofJson::parse("{ \"pages\": [{\"name\": \"" + groupName + "_SubPage\", \"surfaces\": [\"" + searchString + "\"]}]}");
			createCustomPage(pages, midiDevice, customJson);
		} else if (*itDesc == "Color") {
			auto colorCont = contents.find("CONTENTS");
			if (colorCont != contents.end() && colorCont->is_object()) {
				for (auto itColor = colorCont->begin(); itColor != colorCont->end(); ++itColor) {
					auto cdesc = itColor.value().find("DESCRIPTION");
					if (cdesc != itColor.value().end() && cdesc->is_string()) {
						auto s = cdesc->get<std::string>();
						if (s == "Red" || s == "Green" || s == "Blue") {
							page.addParameter(createParameter(itColor.value()));
						}
					}
				}
			}
		} else if (*itDesc == "fx") {
			auto fxCont = contents.find("CONTENTS");
			if (fxCont != contents.end() && fxCont->is_object()) {
				for (auto itFx = fxCont->begin(); itFx != fxCont->end(); ++itFx) {
					auto fdesc = itFx.value().find("DESCRIPTION");
					auto ftype = itFx.value().find("TYPE");
					if (fdesc != itFx.value().end() && ftype != itFx.value().end() && fdesc->is_string() && ftype->is_string() && *fdesc != "FX Type" && *ftype == "f") {
						page.addParameter(createParameter(itFx.value()));
					}
				}
			}
		} else {
			auto ctype = contents.find("TYPE");
			if (keyType == "medias" && ctype != contents.end() && ctype->is_string() && *ctype == "f") {
				page.addParameter(createParameter(contents));
			}
		}
	}
}

void ofxMadOscQuery::oscSendToMadMapper(ofxOscMessage & m) {
	oscSender.sendMessage(m, false);
}

void ofxMadOscQuery::oscReceiveMessages(ofParameterGroup & syncGroup) {
	while (oscReceiver.hasWaitingMessages()) {
		oscReceiver.getParameter(syncGroup);
	}
	//
	//    while(oscReceiver.hasWaitingMessages()){
	//        ofxOscMessage m;
	//        oscReceiver.getNextMessage(m);
	//        ofLog() << "Received message on adress: " << m.getAddress() << endl;
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
MadParameter * ofxMadOscQuery::createParameter(ofJson parameterValues) {
	// cout << parameterValues << endl;
	// cout << endl;
	std::string key = parameterValues["FULL_PATH"];
	parameterMap[key] = MadParameter(parameterValues);
	auto val = &parameterMap.operator[](key);
	ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
	return val;
}
////--------------------------------------------------------------
// MadParameter* ofxMadOscQuery::createParameter(ofJson parameterValues, std::string name){
//     std::string key = parameterValues["FULL_PATH"];
//     parameterMap[key] = MadParameter(parameterValues,name);
//     auto val = &parameterMap.operator[](key);
//     ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
//     return val;
// }

bool ofxMadOscQuery::connectWebSocket(int port) {
	if (!wsClient) wsClient = std::make_unique<OscQueryWebSocketClient>();

	// Connect and set up a message handler that updates parameters.
	return wsClient->connect(ip, port, [this](const std::string& payload) {
		std::lock_guard<std::mutex> lock(paramMutex);
		// naive handler: expect {"FULL_PATH":"/path","VALUE":[v]} JSON
		try {
			ofJson msg = ofJson::parse(payload);
			if (!msg.contains("FULL_PATH") || !msg.contains("VALUE")) return;
			auto path = msg["FULL_PATH"].get<std::string>();
			if (!parameterMap.count(path)) return;
			auto & param = parameterMap[path];
			if (msg["VALUE"].is_array() && !msg["VALUE"].empty()) {
				param.setFromRemoteRaw(msg["VALUE"].at(0));
			}
		} catch (...) {
			// ignore malformed payloads
		}
	});
}

bool ofxMadOscQuery::isWebSocketConnected() const {
	return wsClient != nullptr;
}

void ofxMadOscQuery::disconnectWebSocket() {
	if (wsClient) wsClient->disconnect();
}

void ofxMadOscQuery::subscribeParameter(const std::string & path) {
	if (!wsClient) return;
	ofJson msg;
	msg["COMMAND"] = "LISTEN";
	msg["DATA"] = path;
	wsClient->sendText(msg.dump());
}

void ofxMadOscQuery::subscribeAllParameters() {
	for (auto & kv : parameterMap) {
		subscribeParameter(kv.first);
	}
}
