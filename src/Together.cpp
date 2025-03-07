#include "plugin.hpp"
#include "components.hpp"
#define NUM_ROWS 8
#define NUM_GROUPS 4

struct Together : Module {
	enum ParamId {
		P_TRIG_BUTTON,
		P_RESET_BUTTON,
		P_NUDGE_BUTTON,
		P_NUDGEV,
		P_NudgeMode,
		ENUMS(P_STEPS,NUM_ROWS),
		ENUMS(P_G1,NUM_ROWS),
		ENUMS(P_G2,NUM_ROWS),
		ENUMS(P_G3,NUM_ROWS),
		ENUMS(P_G4,NUM_ROWS),
		ENUMS(P_GNUDGE_BUTTON,NUM_GROUPS),
		ENUMS(P_RNUDGE_BUTTON,NUM_ROWS),	
		ENUMS(P_STEPLOCK,NUM_ROWS),	
		ENUMS(P_RTRIGPROB,NUM_ROWS),	
		P_SNUDGE_BUTTON,
		P_SCALE,
		P_OFFSET,	
		PARAMS_LEN
	};
	enum InputId {
		I_TRIG,
		I_RESET,
		I_NUDGE,
		I_StepCV,
		ENUMS(I_GCV,NUM_GROUPS),
		ENUMS(I_RCV,NUM_ROWS),
		INPUTS_LEN
	};
	enum OutputId {
		O_ORTRIG,
		O_ORGATE,
		ENUMS(O_RTRIG,NUM_ROWS),
		ENUMS(O_RGATE,NUM_ROWS),
		ENUMS(O_G,NUM_GROUPS),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(L_STEP,NUM_ROWS),
		ENUMS(L_STEPLOCK,NUM_ROWS),
		ENUMS(L_STEPACTIVE,NUM_ROWS),
		ENUMS(L_TRIGHEADS,NUM_ROWS),
		ENUMS(L_TRIGTAILS,NUM_ROWS),
		L_RNDMODE,
		L_NUDGEMODE,
		LIGHTS_LEN
	};

	enum NudgeMode {
		NUDGE,
		RANDOM
	};

	dsp::BooleanTrigger resetButton;
	dsp::BooleanTrigger triggerButton;
	dsp::BooleanTrigger grouprndButton[NUM_GROUPS];
	dsp::BooleanTrigger rowrndButton[NUM_ROWS];
	dsp::BooleanTrigger steprndButton;
	dsp::BooleanTrigger stepLockButton[NUM_ROWS];
	dsp::BooleanTrigger nudgeButton;
	dsp::BooleanTrigger nudgemodeButton;
	
	dsp::SchmittTrigger nudgeTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger grouprndTrigger[NUM_GROUPS];
	dsp::SchmittTrigger rowrndTrigger[NUM_ROWS];

	dsp::PulseGenerator orPulse;
	dsp::PulseGenerator rowPulse[NUM_ROWS];

	dsp::ClockDivider LowPriority;

	long seqCount=0;
	bool cycleResult[NUM_ROWS]={true,true,true,true,true,true,true,true};
	float groupValue[NUM_GROUPS]={};
	bool pulse;
	float range=1.f;
	float nudgev=0.05f;
	bool modetoggleready=true;
	int NudgeMode = RANDOM;

	Together() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(P_TRIG_BUTTON, "Trig");
		configButton(P_RESET_BUTTON, "Reset");
		// each row's 
		for(int i=0; i<NUM_ROWS; i++){
			configParam(P_STEPS + i, 0.f, 16.f, 0.f,  string::f("Steps %d", i + 1) );
			paramQuantities[P_STEPS + i]->snapEnabled = true;
			configParam(P_RTRIGPROB + i, 0.f, 1.f, 1.f, string::f("Row %d Trig Probability", i + 1));
			configParam(P_G1 + i, -range, range, 0.f, string::f("Group 1 Row %d", i + 1));
			configParam(P_G2 + i, -range, range, 0.f, string::f("Group 2 Row %d", i + 1));
			configParam(P_G3 + i, -range, range, 0.f, string::f("Group 2 Row %d", i + 1));
			configParam(P_G4 + i, -range, range, 0.f, string::f("Group 2 Row %d", i + 1));
			configOutput(O_RTRIG + i, string::f("Row %d Trig", i + 1));
			configOutput(O_RGATE + i, string::f("Row %d Gate", i + 1));
			configButton(P_RNUDGE_BUTTON + i,string::f("Row %d Randomize", i + 1));
			configButton(P_STEPLOCK + i,string::f("Row %d Step Lock", i + 1));
			configInput(I_RCV + i, string::f("Row %d CV", i + 1));
		}

		configInput(I_NUDGE, "CV Nudge");
		configButton(P_NUDGE_BUTTON,"Manual Nudge");
		configParam(P_NUDGEV, 0.f, 1.f, 0.05f, "Nudge Amount");
		configButton(P_NudgeMode,"Nudge/RND Mode");

		//scale and offset
		configParam(P_SCALE, -5, 5, 1.f, "Scale");
		configParam(P_OFFSET, -5, 5, 0.f, "Offset");
		// the groups
		for(int i=0;i<NUM_GROUPS;i++){
			configInput(I_GCV + i,string::f("Grp CV %d", i + 1));
			configOutput(O_G + i, string::f("Out Grp %d", i + 1));
			configButton(P_GNUDGE_BUTTON + i,string::f("Group %d Randomize", i + 1));
		}
		configButton(P_SNUDGE_BUTTON,"Randomize Steps");
		configInput(I_StepCV, "Step CV");
		configInput(I_TRIG, "Trig");
		configInput(I_RESET, "Reset");
		configOutput(O_ORTRIG, "Global out trig");
		configOutput(O_ORGATE, "Global out gate");

		LowPriority.division=4;
		lights[L_NUDGEMODE].setBrightness(0);
		lights[L_RNDMODE].setBrightness(1);
	}

	float nudge(float pv, float rndammount, float nudgeammount, float lo, float hi, int mode) {
		float nv;
		if(NudgeMode==NUDGE) {
			float v=(random::uniform()>=0.5) ? pv - nudgeammount : pv + nudgeammount;
			nv=clamp(v,lo,hi);
		} else {
			nv= ((rndammount*2)*random::uniform())-rndammount;		}
		return nv;
	}

	void process(const ProcessArgs& args) override {
		// row trigger out pulses
		for(int row=0; row<NUM_ROWS; row++){
			pulse = rowPulse[row].process(args.sampleTime);
			outputs[O_RTRIG + row].setVoltage(pulse ? 10.f : 0.f);
		}

		//global pulse out
		pulse = orPulse.process(args.sampleTime);
		outputs[O_ORTRIG].setVoltage(pulse ? 10.f : 0.f);

		// Reset
		if (resetButton.process(params[P_RESET_BUTTON].getValue()) | resetTrigger.process(inputs[I_RESET].getVoltage(), 0.1f, 2.f)) {
			seqCount = 0;
		}
		
		//lowpriority
		if(LowPriority.process()){
			//nudge or rnd the steps
			if(steprndButton.process(params[P_SNUDGE_BUTTON].getValue())){
				for(int row=0; row<NUM_ROWS; row++){
					//not the locked steps
					if(!params[P_STEPLOCK + row].getValue()) {
						float v=params[P_STEPS + row].getValue();
						params[P_STEPS + row].setValue(nudge(v,16,1,0,16,NudgeMode));
					}
				}
			}

			// nudge row
			nudgev=params[P_NUDGEV].getValue();
			for(int i=0; i<NUM_ROWS;i++){
				float button=rowrndButton[i].process(params[P_RNUDGE_BUTTON +i].getValue());
				float trigger=rowrndTrigger[i].process(inputs[I_RCV + i].getVoltage(), 0.1f, 1.f);
				if(button || trigger){
					params[P_G1 + i].setValue( nudge(params[P_G1 + i].getValue(),range,nudgev,-range,range,NudgeMode ) );
					params[P_G2 + i].setValue( nudge(params[P_G2 + i].getValue(),range,nudgev,-range,range,NudgeMode ) );
					params[P_G3 + i].setValue( nudge(params[P_G3 + i].getValue(),range,nudgev,-range,range,NudgeMode ) );
					params[P_G4 + i].setValue( nudge(params[P_G4 + i].getValue(),range,nudgev,-range,range,NudgeMode ) );			
				}	
			}

			//randomize column
			for(int i=0; i<NUM_GROUPS;i++){
				float button=grouprndButton[i].process(params[P_GNUDGE_BUTTON +i].getValue());
				float trigger=grouprndTrigger[i].process(inputs[I_GCV + i].getVoltage(), 0.1f, 1.f);
				if( button || trigger ) {
					for(int row=0; row<NUM_ROWS; row++){
						if(i==0) params[P_G1 + row].setValue(nudge(params[P_G1 + row].getValue(),range,nudgev,-range,range ,NudgeMode) );
						if(i==1) params[P_G2 + row].setValue(nudge(params[P_G2 + row].getValue(),range,nudgev,-range,range ,NudgeMode) );
						if(i==2) params[P_G3 + row].setValue(nudge(params[P_G3 + row].getValue(),range,nudgev,-range,range ,NudgeMode) );
						if(i==3) params[P_G4 + row].setValue(nudge(params[P_G4 + row].getValue(),range,nudgev,-range,range ,NudgeMode) );					
					}
				}
			}

			//step lock
			for(int row=0; row<NUM_ROWS; row++){
				lights[L_STEPLOCK + row].setBrightness(params[P_STEPLOCK + row].getValue());
			}


			// Mode toggle
			if ( modetoggleready && nudgemodeButton.process(params[P_NudgeMode].getValue())) {
				if(NudgeMode==NUDGE) {
					NudgeMode=RANDOM;
					lights[L_NUDGEMODE].setBrightness(0);
					lights[L_RNDMODE].setBrightness(1);
				} else {
					NudgeMode=NUDGE;
					lights[L_RNDMODE].setBrightness(0);
					lights[L_NUDGEMODE].setBrightness(1);
				}
				modetoggleready=false;
			} else {
				modetoggleready=true;
			}

			// nudge
			if (nudgeButton.process(params[P_NUDGE_BUTTON].getValue()) | nudgeTrigger.process(inputs[I_NUDGE].getVoltage(), 0.1f, 2.f)) {
				nudgev=params[P_NUDGEV].getValue();
				for(int i=0; i<NUM_GROUPS;i++){
					for(int row=0; row<NUM_ROWS; row++){
						if(i==0) params[P_G1 + row].setValue(nudge( params[P_G1 + row].getValue(),range,nudgev,-range,range ,NudgeMode));
						if(i==1) params[P_G2 + row].setValue(nudge( params[P_G2 + row].getValue(),range,nudgev,-range,range ,NudgeMode));
						if(i==2) params[P_G3 + row].setValue(nudge( params[P_G3 + row].getValue(),range,nudgev,-range,range ,NudgeMode));
						if(i==3) params[P_G4 + row].setValue(nudge( params[P_G4 + row].getValue(),range,nudgev,-range,range ,NudgeMode));					
					}
				}
			}
		}
		float b_trigger=params[P_TRIG_BUTTON].getValue();
		float i_trigger=inputs[I_TRIG].getVoltage();
		if( clockTrigger.process(i_trigger) | triggerButton.process(b_trigger)){
			seqCount++;

			//what is trigging this cycle...
			for(int i=0; i<NUM_ROWS; i++){

				// row pulse
				bool pulse = rowPulse[i].process(args.sampleTime);
				outputs[O_RTRIG + i].setVoltage(pulse ? 10.f : 0.f);			

				//check if row is triggered based on steps
				//just note which rows will trigger, do it later
				int v=(int)params[P_STEPS + i].getValue();
				if(v==0) {  
			 		cycleResult[i] = false;
					lights[L_STEPACTIVE + i].setBrightness(0);
				} else {
					cycleResult[i] = ((seqCount % v)==0) ? true : false;
					lights[L_STEPACTIVE +i].setBrightness(1);
				}
			}

			float v=1;
			//reset column value
			for( int i=0; i<NUM_GROUPS; i++) groupValue[i]=0;

			//process columns
			// sum the column, if row is triggered
			bool anytrig=false;
			for(int row=0; row<NUM_ROWS; row++){
				if(cycleResult[row]) {	
					//triggers and gates
					//check prob
					if(random::uniform() <= params[P_RTRIGPROB + row].getValue() ){
						anytrig=true;
						rowPulse[row].trigger(1e-3f);
						outputs[O_RGATE + row].setVoltage(10.f);
						lights[L_TRIGHEADS +row].setBrightness(1);
						lights[L_TRIGTAILS +row].setBrightness(0);
					} else {
						lights[L_TRIGHEADS +row].setBrightness(0);
						lights[L_TRIGTAILS +row].setBrightness(1);
					}
					//add the triggered values			
					groupValue[0]+=v * params[P_G1 + row].getValue();
					groupValue[1]+=v * params[P_G2 + row].getValue();
					groupValue[2]+=v * params[P_G3 + row].getValue();
					groupValue[3]+=v * params[P_G4 + row].getValue();
					lights[L_STEP + row].setBrightness(1);


				} else {
					outputs[O_RGATE + row].setVoltage(0.f);	
					lights[L_STEP + row].setBrightness(0);		
				}
			}

			if(anytrig) {
				orPulse.trigger(1e-3f);
				outputs[O_ORGATE].setVoltage(10.f);

			} else {
				outputs[O_ORGATE].setVoltage(0.f);
			}

			//set resulting value
			for( int i=0; i<NUM_GROUPS; i++) {
				//apply scale and offset
				v=groupValue[i]*params[P_SCALE].getValue();
				v+=params[P_OFFSET].getValue();
				outputs[O_G + i].setVoltage(v);
			}
		}
	}
};

struct TogetherWidget : ModuleWidget {
	TogetherWidget(Together* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Together.svg")));

		float width=98.56;
		float x,y;
		float s=10;
		float yoff=26;
		float lx=7.62;

		x=width-lx-10;
		addInput(createInputCentered<CoffeeInputPortButton>(mm2px(Vec(x, 16)), module, Together::I_TRIG));
		addParam(createParamCentered<CoffeeTinyButton>(mm2px(Vec(x+3.5, 16-3.5)), module, Together::P_TRIG_BUTTON));
		x=width-lx;
		addInput(createInputCentered<CoffeeInputPortButton>(mm2px(Vec(x, 16)), module, Together::I_RESET));
		addParam(createParamCentered<CoffeeTinyButton>(mm2px(Vec(x+3.5, 16-3.5)), module, Together::P_RESET_BUTTON));

		for(int i=0; i<NUM_ROWS; i++){
			x=lx;
			y=yoff+(i*s);
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x, y)), module, Together::P_STEPS + i));
			addParam(createParamCentered<CoffeeTinyButtonLatch>(mm2px(Vec(x-4, y-4)), module, Together::P_STEPLOCK + i ));
			addChild(createLightCentered<CoffeeTinySimpleLight<OrangeLight>>(mm2px(Vec(x-4, y-4)), module, Together::L_STEPLOCK + i));
			x=5+lx+(s*1);
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, y)), module, Together::P_G1 + i));
			x=5+lx+(s*2);
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, y)), module, Together::P_G2 + i));		
			x=5+lx+(s*3);
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, y)), module, Together::P_G3 + i));
			x=5+lx+(s*4);
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, y)), module, Together::P_G4 + i));

			x=5+lx+(s*5);
			addInput(createInputCentered<CoffeeInputPortButton>(mm2px(Vec(x, y)), module, Together::I_RCV + i));
			addParam(createParamCentered<CoffeeTinyButton>(mm2px(Vec(x+3.5, y-3.5)), module, Together::P_RNUDGE_BUTTON + i ));

			x=5+lx+(s*6);
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, y)), module, Together::P_RTRIGPROB + i));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(x-3, y+3.5)), module, Together::L_TRIGTAILS + i));
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(x+3, y+3.5)), module, Together::L_TRIGHEADS + i));

			// row outs trig
			x=width-lx-10;
			addOutput(createOutputCentered<CoffeeOutputPort>(mm2px(Vec(x, y)), module, Together::O_RTRIG + i));
			// row outs gate
			x=width-lx;
			addOutput(createOutputCentered<CoffeeOutputPort>(mm2px(Vec(x, y)), module, Together::O_RGATE + i));

			addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(lx+(s-2), y)), module, Together::L_STEP + i));
			addChild(createLightCentered<TinyLight<YellowLight>>(mm2px(Vec(lx+5, y)), module, Together::L_STEPACTIVE + i));
		}

		//Step CV input
		y=16;
		x=lx;
		addInput(createInputCentered<CoffeeInputPortButton>(mm2px(Vec(x, y)), module, Together::I_StepCV));
		addParam(createParamCentered<CoffeeTinyButton>(mm2px(Vec(x+3.5, y-3.5)), module, Together::P_SNUDGE_BUTTON));

		//nudge vs rnd switch
		y=16;
		x=lx+7.5;
		addParam(createParamCentered<CoffeeButtonVertIndicator>(mm2px(Vec(x, y)), module, Together::P_NudgeMode));
		addChild(createLightCentered<TinyLight<OrangeLight>>(mm2px(Vec(x, y)), module, Together::L_NUDGEMODE));
		addChild(createLightCentered<TinyLight<OrangeLight>>(mm2px(Vec(x, y+2)), module, Together::L_RNDMODE));


		// group inputs
		for(int i=0; i<NUM_GROUPS; i++){
			y=16;
			x=lx+(s*i)+s+5;
			addInput(createInputCentered<CoffeeInputPortButton>(mm2px(Vec(x, y)), module, Together::I_GCV + i));
			addParam(createParamCentered<CoffeeTinyButton>(mm2px(Vec(x+3.5, y-3.5)), module, Together::P_GNUDGE_BUTTON + i ));
		}

		//nudge
		y=16;
		x=lx+(s*4)+s+5;
		addInput(createInputCentered<CoffeeInputPortButton>(mm2px(Vec(x, y)), module, Together::I_NUDGE));
		addParam(createParamCentered<CoffeeTinyButton>(mm2px(Vec(x+3.5, y-3.5)), module, Together::P_NUDGE_BUTTON));
		x=lx+(s*5)+s+5;		
		addParam(createParamCentered<Trimpot>(mm2px(Vec(x, y)), module, Together::P_NUDGEV));

		// group outputs
		for(int i=0; i<NUM_GROUPS; i++){
			y=yoff+(s*NUM_ROWS);
			y=112;
			x=lx+(s*i)+s+5;
			addOutput(createOutputCentered<CoffeeOutputPort>(mm2px(Vec(x, y)), module, Together::O_G + i));
		}

		//scale and offset
		x=lx;
		y=112;
		addParam(createParamCentered<Trimpot>(mm2px(Vec(x-3, y-3)), module, Together::P_SCALE));		
		addParam(createParamCentered<Trimpot>(mm2px(Vec(x+3, y+3)), module, Together::P_OFFSET));		

		//global or trig and gate
		y=112;
		x=width-lx-10;
		addOutput(createOutputCentered<CoffeeOutputPort>(mm2px(Vec(x,y)), module, Together::O_ORTRIG));
		x=width-lx;
		addOutput(createOutputCentered<CoffeeOutputPort>(mm2px(Vec(x,y)), module, Together::O_ORGATE));
	}
};


Model* modelTogether = createModel<Together, TogetherWidget>("Together");