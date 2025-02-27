#include "plugin.hpp"

struct Travel : Module {
	enum ParamId {
		P_TRIG1,
		P_DURATION,
		P_DURATIONSCALE,
		P_SCALE1,
		P_OFFSET1,
		P_SCALE2,
		P_OFFSET2,
		P_SHAPE,
		PARAMS_LEN
	};
	enum InputId {
		I_TRIG,
		I_DURATION,
		I_SHAPE,
		I_CV1,
		I_CV2,
		INPUTS_LEN
	};
	enum OutputId {
		O_CV1,
		O_EOC,
		OUTPUTS_LEN
	};
	enum LightId {
		L_PROCESS,
		LIGHTS_LEN
	};

    dsp::SchmittTrigger clockTrigger;
	dsp::BooleanTrigger buttonTrigger;
	dsp::PulseGenerator eocPulse;

	bool cycling=false;
	float cycleProgress=0.f;
	float cycleStart=0.f;
	int durationScales[3] = {1, 10, 100};
	float i_CV2=0.f;		
	float i_CV1=0.f;
	float p_shape=0.f;
	float targetDuration=0.f;
	bool track=false;

	Travel() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(P_TRIG1, "Manual Trigger");
		configSwitch(P_DURATIONSCALE, 0, 2, 0, "Duration scale", {"1", "10", "100"});
		configParam(P_DURATION, 0.f, 1.f, 1.f, "Duration");
		configParam(P_OFFSET1, -5.0f, 5.0f, 0.0f, "Offset", " V");
		configParam(P_SCALE1, 0.0f, 1.0f, 1.0f, "Scale", "%", 0.0f, 100.0f);
		configParam(P_SCALE2, 0.0f, 1.0f, 1.0f, "Scale", "%", 0.0f, 100.0f);
		configParam(P_OFFSET2, -5.0f, 5.0f, 0.0f, "Offset", " V");
		configParam(P_SHAPE, -5.0f, 5.0f, 0.0f, "Shape");
		configInput(I_TRIG, "Trigger");
		configInput(I_DURATION, "Duration");
		configInput(I_SHAPE, "Shape");
		configInput(I_CV1, "CV1");
		configInput(I_CV2, "CV2");
		configOutput(O_EOC, "End of Cycle");
		configOutput(O_CV1, "Output");
	}

	void updateInputs(const ProcessArgs& args) {
		float p_scale1=params[P_SCALE1].getValue();
		float p_offset1=params[P_OFFSET1].getValue();

		float p_scale2=params[P_SCALE2].getValue();
		float p_offset2=params[P_OFFSET2].getValue();
		int p_durationscale=params[P_DURATIONSCALE].getValue();
		//get inputs
		i_CV1=(inputs[I_CV1].getVoltage() * p_scale1) + p_offset1;
		i_CV2=(inputs[I_CV2].getVoltage() * p_scale2) + p_offset2;

		//get duration
		targetDuration=params[P_DURATION].getValue() * durationScales[p_durationscale];
		if(inputs[I_DURATION].isConnected()){
			targetDuration=inputs[I_DURATION].getVoltage() * durationScales[p_durationscale];
		}

		//get shape
		p_shape=params[P_SHAPE].getValue()/5;
		if(inputs[I_SHAPE].isConnected()){
			p_shape=clamp(inputs[I_SHAPE].getVoltage(),-5.f, 5.f)/5;
		}
	}

	void process(const ProcessArgs& args) override {

		bool pulse = eocPulse.process(args.sampleTime);
		outputs[O_EOC].setVoltage(pulse ? 10.f : 0.f);

		// start the cycle
		float b_trigger=params[P_TRIG1].getValue();
		float i_trigger=inputs[I_TRIG].getVoltage();
		if( (!cycling | track) && (clockTrigger.process(i_trigger) | buttonTrigger.process(b_trigger))){
			cycleStart=args.frame;
			cycling=true;
		} 

		if((cycling && track) | !cycling ){
			updateInputs(args);
		}





		if(cycling){
			cycleProgress=((args.frame-cycleStart)/args.sampleRate)/targetDuration;
			lights[L_PROCESS].setBrightness(1-cycleProgress);
			if(cycleProgress<1) {

				float exp1=pow(2,10*(cycleProgress-1));
				float exp2=(1-pow(2,-10*cycleProgress ));
				float v,v1,v2;
				v1=i_CV1-((i_CV1 - i_CV2)*cycleProgress); //linear

				// -1 < p_shape < 1
				if(p_shape >= 0){
					v2=i_CV1-((i_CV1 - i_CV2)*exp1);
					v=(v2*p_shape)+(v1*(1-p_shape));
				} else {
					v2=i_CV1-((i_CV1 - i_CV2)*exp2);
					v=(v2*abs(p_shape))+(v1*(1-abs(p_shape)));
				}
				outputs[O_CV1].setVoltage(v);
			} else {  //cycle is finished
				cycling =false;
				eocPulse.trigger(1e-3f);
			}
		}
	}
};	



struct TravelWidget : ModuleWidget {
	TravelWidget(Travel* module) {
		setModule(module);
		float lx=30.48/4;
		float rx=30.48-lx;
		float mx=30.48/2;
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Travel.svg")));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lx, 15)), module, Travel::I_TRIG));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(rx, 15)), module, Travel::P_TRIG1));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lx, 30)), module, Travel::I_DURATION));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(rx, 30)), module, Travel::P_DURATION));
		addParam(createParamCentered<CKSSThreeHorizontal>(mm2px(Vec(mx, 37.5)), module, Travel::P_DURATIONSCALE));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(mx, 30)), module, Travel::L_PROCESS));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(rx, 50)), module, Travel::P_SHAPE));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lx, 50)), module, Travel::I_SHAPE));

		float y=70;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lx, y)), module, Travel::I_CV1));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(lx, y+10)), module, Travel::P_SCALE1));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(lx, y+20)), module, Travel::P_OFFSET1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(rx, y)), module, Travel::I_CV2));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(rx, y+10)), module, Travel::P_SCALE2));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(rx, y+20)), module, Travel::P_OFFSET2));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(lx, 112)), module, Travel::O_CV1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(rx, 112)), module, Travel::O_EOC));
	}
	void appendContextMenu(Menu* menu) override {
			Travel* module = dynamic_cast<Travel*>(this->module);
			assert(module);

			menu->addChild(new MenuSeparator());
			menu->addChild(createSubmenuItem("Track/Hold Inputs", "", [=](Menu* menu) {
				Menu* trackholdMenu = new Menu();
				trackholdMenu->addChild(createMenuItem("Track", CHECKMARK(module->track == true), [module]() { module->track = true; }));
				trackholdMenu->addChild(createMenuItem("Hold", CHECKMARK(module->track == false), [module]() { module->track = false; }));
				menu->addChild(trackholdMenu);
			}));
		}	
};




Model* modelTravel = createModel<Travel, TravelWidget>("Travel");
