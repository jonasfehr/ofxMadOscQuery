//
//  MadParameter.h
//  MadMapper_oscQuery
//
//  Created by Jonas Fehr on 06/04/2018.
//

#ifndef MadParameter_h
#define MadParameter_h

//#include "ofMain.h"
#include "ofMain.h"
#include "MidiComponent.h"
#include "ofxOsc.h"
#include <vector>
#include <chrono>
#include <limits>

class MadParameter : public ofParameter<float>{
public:
//    MadParameter(string name, float value,  float min, float max){
//        this->setName(name);
//        range.min = min;
//        range.max = max;
//
//        float valueNormalized = ofMap(value, range.min, range.max, 0, 1);
//        this->set(valueNormalized);
//
//        bSelectable = false;
//        bIsGroup = false;
//    };
	
	MadParameter(ofJson parameterValues, bool doSendOsc = true){
		auto fpIt = parameterValues.find("FULL_PATH");
		this->setOscAddress((fpIt != parameterValues.end() && fpIt->is_string()) ? fpIt->get<std::string>() : "");
		auto descIt = parameterValues.find("DESCRIPTION");
		this->setName((descIt != parameterValues.end() && descIt->is_string()) ? descIt->get<std::string>() : "");
		auto typeIt = parameterValues.find("TYPE");
		if(typeIt != parameterValues.end() && typeIt->is_string()){
			parameterType = typeIt->get<std::string>();
		}
		if(!parameterValues["RANGE"].is_null() ){
			range.min = parameterValues["RANGE"].at(0)["MIN"].get<float>();
			range.max = parameterValues["RANGE"].at(0)["MAX"].get<float>();
		}
		float rawValue = 0.f;
		if(!parameterValues["VALUE"].is_null() && parameterValues["VALUE"].is_array() && !parameterValues["VALUE"].empty()){
			rawValue = parameterValues["VALUE"].at(0).get<float>();
		}
		float valueNormalized = ofMap(rawValue, range.min, range.max, 0, 1, true);
		this->set(valueNormalized);
		
		bSelectable = false;
		if(parameterValues["DESCRIPTION"] == "Opacity") bSelectable = true;
		bIsGroup = false;
		
		checkIfOpacityParameter(this->getOscAddress());
		
		this->doSendOsc = doSendOsc;
	};
	
	MadParameter(const MadParameter& other) {
		// Copy basic parameter properties
		this->set(other);
		
		// Copy custom properties
		this->bSelectable = other.bSelectable;
		this->bIsGroup = other.bIsGroup;
		this->doSendOsc = other.doSendOsc;
		this->range = other.range;
		this->updateFromMidi = other.updateFromMidi;
		this->oscAddress = other.oscAddress;
		this->isOpacityParameter = other.isOpacityParameter;
		this->isFixtureParameter = other.isFixtureParameter;
		this->isModuleParameter = other.isModuleParameter;
		this->isMaster = other.isMaster;
		this->connectedMedia = other.connectedMedia;
		this->parentName = other.parentName;
		this->parameterType = other.parameterType;
	}
	
	MadParameter& operator=(const MadParameter& other) {
		if (this != &other) {  // Check for self-assignment
			// Copy basic properties
			this->set(other.get());
			this->bSelectable = other.bSelectable;
			this->bIsGroup = other.bIsGroup;
			this->doSendOsc = other.doSendOsc;
			this->range = other.range;
			this->updateFromMidi = other.updateFromMidi;
			this->oscAddress = other.oscAddress;
			this->isOpacityParameter = other.isOpacityParameter;
			this->isFixtureParameter = other.isFixtureParameter;
			this->isModuleParameter = other.isModuleParameter;
			this->isMaster = other.isMaster;
			this->connectedMedia = other.connectedMedia;
			this->parentName = other.parentName;
			this->parameterType = other.parameterType;
		}
		return *this;
	}
	
//    MadParameter(ofJson parameterValues, string name){
//        this->setOscAddress(parameterValues["FULL_PATH"].get<std::string>());
//        this->setName(name);
//        if(!parameterValues["RANGE"].is_null() ){
//            range.min = parameterValues["RANGE"].at(0)["MIN"].get<float>();
//            range.max = parameterValues["RANGE"].at(0)["MAX"].get<float>();
//        }
//        float valueNormalized = ofMap(parameterValues["VALUE"].at(0), range.min, range.max, 0, 1);
//        this->set(valueNormalized);
//
//        bSelectable = false;
//        if(parameterValues["DESCRIPTION"] == "Opacity") bSelectable = true;
//        bIsGroup = false;
//    };

	MadParameter(){};
	~MadParameter(){};
	
	float getParameterValue(){
		float value = ofMap(this->get(), 0, 1, range.min, range.max, true);
		return value;
	}

	int getParameterIntValue(){
		return static_cast<int>(std::round(getParameterValue()));
	}
	
	// ── Encoder-relative acceleration ────────────────────────────────────────────
	// When linked to a CMT_CONTROL_CHANGE_ENCODER_RELATIVE component, incoming
	// values are treated as an accumulator. We extract the per-tick delta, scale
	// it by a velocity factor, and apply it to this->get() (the MM-synced value).
	// After each tick the accumulator is reset to the computed value so it never
	// diverges from the parameter — this prevents boundary oscillation.
	bool          encoderAccelEnabled  = false;
	float         encoderAccelBase     = 6.f;   // ticks/sec at which accel begins
	float         encoderAccelMax      = 8.f;   // top-end delta multiplier
	float         encoderSensitivity   = 0.8f;  // global scale on applied deltas
	float         encoderPrevNorm      = 0.f;   // last accumulator position (post-reset)
	bool          encoderResetting     = false; // guard against recursive callback
	MidiComponent* linkedEncoder       = nullptr; // component to reset after each tick
	std::chrono::steady_clock::time_point encoderLastStamp {};

	// Direction-lock to avoid tug-of-war: last source + timestamp
	enum class InputSource { None, Midi, Remote };
	InputSource currentMaster = InputSource::None;
	std::chrono::steady_clock::time_point masterStamp { std::chrono::steady_clock::now() };
	std::chrono::milliseconds masterWindow { 200 };
	bool suppressOscSend = false;
	bool inRemoteUpdate = false;
	std::chrono::steady_clock::time_point lastOscSendStamp { std::chrono::steady_clock::now() - std::chrono::milliseconds(1000) };
	std::chrono::milliseconds minOscSendInterval { 8 };
	float lastSentFloat = std::numeric_limits<float>::quiet_NaN();
	int lastSentInt = std::numeric_limits<int>::min();
	// Use when applying values received from remote OSCQuery (raw units)
	void setFromRemoteRaw(float raw){
		auto now = std::chrono::steady_clock::now();
		// If current master is MIDI and still within window, ignore remote
		if (currentMaster == InputSource::Midi && (now - masterStamp) < masterWindow) return;
		currentMaster = InputSource::Remote;
		masterStamp = now;

		inRemoteUpdate = true;
		this->set(ofMap(raw, range.min, range.max, 0, 1, true));
		inRemoteUpdate = false;
	}
	
	std::string getParameterName(){
		// returns last part of osc address
		std::string oscAddress = this->getOscAddress();
		std::vector<std::string> seglist;
		std::stringstream ss(oscAddress);
		std::string segment;
		while(std::getline(ss, segment, '/')){
			seglist.push_back(segment);
		}
		return seglist.at(seglist.size()-1);
	}
	
	// Display-friendly name using flags: parent for master, third-from-last for module, else last
	std::string getDisplayParameterName(){
		std::string oscAddress = this->getOscAddress();
		if(oscAddress.empty()) return this->getName();
		std::vector<std::string> seglist;
		std::stringstream ss(oscAddress);
		std::string segment;
		while(std::getline(ss, segment, '/')){
			if(!segment.empty()) seglist.push_back(segment);
		}
		if(seglist.empty()) return this->getName();
		if (isMaster) {
			if(seglist.size() >= 2) return seglist[seglist.size()-2];
			return this->getName();
		} else if (isModuleParameter) {
			if(seglist.size() >= 3) return seglist[seglist.size()-3];
			return this->getName();
		}
		return seglist.back();
	}
	
	bool bSelectable = false;
	bool isSelectable(){return bSelectable;};
	
	 ofEvent<ofxOscMessage> oscSendEvent;

	string oscAddress;
	void setOscAddress(string address){ oscAddress = address;}
	string getOscAddress(){ return oscAddress;}

	struct Range{
		float min = 0.f;
		float max = 1.f;
	} range;

	std::string parameterType = "f";
	
	bool isGroup(){return bIsGroup;}
	void setIsGroup(bool isGroup){bIsGroup = isGroup;}
	bool bIsGroup;
	
	bool updateFromMidi = false;

//    // Send OSC when parameter changed
	void onParameterChange(float & p){
		// Guard: ignore the recursive callback triggered when we reset the accumulator below
		if (encoderResetting) return;

		if (encoderAccelEnabled) {
			// Clamp incoming p to [0,1]: ofParameter<float> with no explicit range can
			// briefly hold an out-of-range value before the explicit clamping assignment
			// in MidiComponent fires a second event. Without this clamp the second event
			// produces a wrong-sign delta and the value oscillates near the boundaries.
			const float pClamped = std::max(0.f, std::min(1.f, p));
			const float delta = pClamped - encoderPrevNorm;

			if (delta != 0.f) {
				auto now = std::chrono::steady_clock::now();
				auto dtMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - encoderLastStamp).count();
				encoderLastStamp = now;

				float accel = 1.f;
				if (dtMs > 0 && dtMs < 500) {
					const float vel = 1000.f / static_cast<float>(dtMs);
					accel = std::min(vel / encoderAccelBase, encoderAccelMax);
					if (accel < 1.f) accel = 1.f;
				}
				// Apply accelerated delta to the MM-synced value, not the raw accumulator.
				p = std::max(0.f, std::min(1.f, this->get() + delta * accel * encoderSensitivity));

				// Reset the encoder accumulator to the computed value so it stays in sync
				// with the parameter. Without this the accumulator diverges (it only moves
				// by one raw step per tick while the parameter moves by accel steps), which
				// causes runaway behavior once the accumulator hits the 0/1 rail.
				if (linkedEncoder) {
					encoderResetting = true;
					linkedEncoder->value = p;
					encoderResetting = false;
				}
			} else {
				p = pClamped;
			}
			encoderPrevNorm = p;
		}

		updateFromMidi = true;
		this->set(p);

		if(inRemoteUpdate){
			updateFromMidi = false;
			return;
		}

		auto now = std::chrono::steady_clock::now();
		if ((now - lastOscSendStamp) < minOscSendInterval) {
			updateFromMidi = false;
			return;
		}

		currentMaster = InputSource::Midi;
		masterStamp = std::chrono::steady_clock::now();

		if(doSendOsc){
			ofxOscMessage m;
			
			if(this->isOpacityParameter){
				auto newAddress = this->oscAddress;
				auto start_position_to_erase = newAddress.find("opacity");
				if(start_position_to_erase != std::string::npos){
					newAddress.erase(start_position_to_erase, ofToString("opacity").size());
					newAddress += "visible";
					
					m.clear();
					m.setAddress(newAddress);
					if(p>0) m.addIntArg(1);
					else    m.addIntArg(0);
					ofNotifyEvent(oscSendEvent,m,this);
				}
			}
			if(this->isFixtureParameter){
				auto newAddress = this->oscAddress;
				auto start_position_to_erase = newAddress.find("luminosity");
				if(start_position_to_erase != std::string::npos){
					newAddress.erase(start_position_to_erase, ofToString("luminosity").size());
					newAddress += "visible";
					
					m.clear();
					m.setAddress(newAddress);
					if(p>0) m.addIntArg(1);
					else    m.addIntArg(0);
					ofNotifyEvent(oscSendEvent,m,this);
				}
			}

			m.clear();
			m.setAddress(oscAddress);
			if(parameterType == "i"){
				int v = getParameterIntValue();
				if (v == lastSentInt) {
					updateFromMidi = false;
					return;
				}
				m.addIntArg(v);
				lastSentInt = v;
			}else{
				float v = getParameterValue();
				if (!std::isnan(lastSentFloat) && std::fabs(v - lastSentFloat) < 1e-6f) {
					updateFromMidi = false;
					return;
				}
				m.addFloatArg(v);
				lastSentFloat = v;
			}
			ofNotifyEvent(oscSendEvent,m,this);
			lastOscSendStamp = now;
		}
		updateFromMidi = false;

	}
	

	void checkIfOpacityParameter(std::string oscAddress){
		// checks whether this parameter controls opacity
		// remove starting '/'
		if(!oscAddress.empty() && oscAddress[0] == '/'){
			oscAddress = oscAddress.substr(1);
		}
		std::stringstream ss(oscAddress);
		std::string segment;
		std::vector<std::string> segList;
		while(std::getline(ss, segment, '/')){
			segList.push_back(segment);
		}
		if(segList.empty()) return;

		const std::string &last = segList.back();
		
		if(last == "opacity"){
			this->isOpacityParameter = true;
			
			isMaster = true;
			if(segList.size()>=2) parentName = segList[segList.size()-2];
		}
		
		if(last == "luminosity"){
			this->isFixtureParameter = true;
			
			isMaster = true;
			if(segList.size()>=2) parentName = segList[segList.size()-2];
		}
		
		if(last == "Float_Value"){
			this->isModuleParameter = true;
			
			isMaster = false;
			if(segList.size()>=2) parentName = segList[segList.size()-2];
		}
		
	}
	
	void linkMidiComponent(MidiComponent &midiComponent){
		encoderAccelEnabled = (midiComponent.controlMessageType == CMT_CONTROL_CHANGE_ENCODER_RELATIVE);
		linkedEncoder       = encoderAccelEnabled ? &midiComponent : nullptr;
		encoderPrevNorm     = this->get();
		encoderLastStamp    = std::chrono::steady_clock::now();
		midiComponent.value = this->get();
		midiComponent.value.addListener(this, &MadParameter::onParameterChange);
	}

	void unlinkMidiComponent(MidiComponent &midiComponent){
		midiComponent.value.removeListener(this, &MadParameter::onParameterChange);
		linkedEncoder = nullptr;
	}
	
	
	
	bool isOpacityParameter = false;
	bool isFixtureParameter = false;
	bool isModuleParameter = false;
	bool isMaster = false;
	string connectedMedia;
	void setConnectedMediaName( string name ){ this->connectedMedia = name; };
	string getConnectedMediaName(){ return this->connectedMedia; };
	string parentName;
	
	bool doSendOsc;
	
	
};

#endif /* MadParameter_h */
