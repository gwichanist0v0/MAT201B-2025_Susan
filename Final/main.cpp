#include <cstdio> // for printing to stdout
#include <stdio.h>
#include <map>
#include <vector>
#include <string>
#include <set>
#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Gamma.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Types.h"
#include "Gamma/DFT.h"

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"
#include "al/io/al_MIDI.hpp"
#include "al/math/al_Random.hpp"
#include "al_ext/assets3d/al_Asset.hpp"


using namespace al;
using namespace std;
#define FFT_SIZE 4048

//Self reminders: Needs to initialize a chord & melody 

class NoteDecision 
{
  public: 
    
    int currentChordIndex = 0; // Start on "i"

    
    //Self reminder add the 7th note
    //V and V 64 are same chord and needs update since previous code bass note is always root
    std::map<std::string, std::vector<int>> chordDictionary = {
      { "i",    {48, 52, 55, 36, 40, 43} },  // C E G (C3 + C2) 1 maj
      { "ii",   {50, 54, 57, 38, 42, 45} },  // D F# A 2 maj
      { "iii",  {52, 55, 59, 40, 43, 47} },  // E G B 3 min
      { "iv",   {54, 57, 60, 42, 45, 48} },  // F# A C 4 min
      { "v",    {55, 59, 50, 43, 47, 38} },  // G B D 5 maj
      { "v64",  {50, 55, 59, 38, 43, 47} },  // D G B 5 64 maj
      { "vi",   {57, 60, 64, 45, 48, 52} },  // A C E 6 min
      { "vii",  {59, 62, 57, 47, 50, 45} }   // B D A 7 dim
    };


    std::map<std::string, std::vector<int>> melodyDictionary = {
      { "i",    {60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84} }, // C major (Ionian)
      { "ii",   {62, 64, 66, 67, 69, 71, 73, 74, 76, 78, 79, 81, 83, 85, 86} }, // D major
      { "iii",  {64, 66, 67, 69, 71, 73, 75, 76, 78, 79, 81, 83, 85, 87, 88} }, // E minor
      { "iv",   {66, 67, 69, 71, 72, 74, 76, 78, 79, 81, 83, 84, 86, 88, 90} }, // F# minor
      { "v",    {67, 69, 71, 72, 74, 76, 78, 79, 81, 83, 84, 86, 88, 90, 91} }, // G major
      { "v64",  {62, 64, 66, 67, 69, 71, 73, 74, 76, 78, 79, 81, 83, 85, 86} }, // D major (again)
      { "vi",   {69, 71, 72, 74, 76, 78, 79, 81, 83, 84, 86, 88, 90, 91, 93} }, // A minor
      { "vii",  {71, 72, 74, 76, 78, 79, 81, 83, 84, 86, 88, 90, 91, 93, 95} }  // B diminished (used like Locrian)
    };

    
    std::vector<std::string> chordNames = {"i", "ii", "iii", "iv", "v", "v64", "vi", "vii"};
    static const int numChords = 8;
    float harmMarkov[numChords][numChords] = {
        {0.0f, 0.32f, 0.19f, 0.05f, 0.1f, 0.04f, 0.26f, 0.04f},
        {0.25f, 0.0f, 0.0f, 0.0f, 0.45f, 0.2f, 0.0f, 0.1f},
        {0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f},
        {0.15f, 0.1f, 0.0f, 0.0f, 0.35f, 0.25f, 0.0f, 0.15f},
        {0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f},
        {0.1f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f},
        {0.5f, 0.0f, 0.0f, 0.0f, 0.2f, 0.3f, 0.0f, 0.0f}
    };

    int pickNextChordIndex() {
      float r = rnd::uniform();
      float sum = 0.0f;
      for (int i = 0; i < numChords; ++i) {
          sum += harmMarkov[currentChordIndex][i];
          if (r <= sum) {
              return i;
          }
      }
      return currentChordIndex; // fallback
    }

    std::vector<int> getNextHarmonyChord() {
      currentChordIndex = pickNextChordIndex();
      std::string chordName = chordNames[currentChordIndex];
      std::vector<int> allNotes = chordDictionary[chordName];

      std::random_shuffle(allNotes.begin(), allNotes.end());

      std::vector<int> selectedNotes;
      std::set<int> seen; 
      for (int note : allNotes) {
          if (seen.find(note) == seen.end()) {
              selectedNotes.push_back(note);
              seen.insert(note);
          }
          if (selectedNotes.size() == 2) break;
      }

      while (selectedNotes.size() < 2) {
          int filler = rnd::uniform(48, 72);
          if (seen.find(filler) == seen.end()) {
              selectedNotes.push_back(filler);
          }
      }

      return selectedNotes;
    }

  std::vector<int> generateLSystemMelody(const std::string &chordName) {
    // Terminal symbols: 5–15
    std::map<int, std::vector<int>> rules = {
        {0, {1, 2}},        // initial branching
        {1, {5, 6}},        // terminal
        {2, {3, 4}},        // leads to terminals
        {3, {7, 8}},        // terminal
        {4, {9, 10}},       // terminal
        {5, {}}, {6, {}}, {7, {}}, {8, {}}, {9, {}}, {10, {}},
        {11, {}}, {12, {}}, {13, {}}, {14, {}}, {15, {}}
    };

    std::vector<int> melody;
    std::vector<int> tree;

    // Start with a safe axiom
    tree.push_back(0);

    int iterations = 0;
    const int maxIterations = 20;

    while (!tree.empty() && melody.size() < 10 && iterations < maxIterations) {
        int current = tree.front();
        tree.erase(tree.begin());

        if (rules.find(current) != rules.end() && !rules[current].empty()) {
            std::vector<int> expansion = rules[current];
            std::random_shuffle(expansion.begin(), expansion.end());
            tree.insert(tree.end(), expansion.begin(), expansion.end());
        } else {
            melody.push_back(current);
        }

        iterations++;
    }

    // Fallback
    if (melody.empty()) {
        melody.push_back(rnd::uniform(5, 16)); // pick a terminal
    }

    // Convert to melody MIDI notes from melodyDictionary
    const std::vector<int> &scale = melodyDictionary[chordName];
    std::vector<int> finalMelody;
    for (int symbol : melody) {
        int index = symbol % scale.size(); // safely wrap
        finalMelody.push_back(scale[index]);
    }

    return finalMelody;
  }






};



class Harm : public SynthVoice 
{
public:
// Unit generators
  gam::Pan<> mPan;
  gam::Sine<> mOsc;
  gam::ADSR<> mAmpEnv;
  // envelope follower to connect audio output to graphics
  gam::EnvFollow<> mEnvFollow;
  // Draw parameters
  Mesh mHarm;
  double rotateA;
  double rotateB;
  double spin = al::rnd::uniformS();
  double timepose = 0;
  Vec3f note_position;
  Vec3f note_direction;

  void init() override
  {
    mAmpEnv.curve(0); // linear segments
    mAmpEnv.levels(0, 1, 1, 0);
    mAmpEnv.sustainPoint(2);
 
    //      mVibEnv.curve(0);
    addSphere(mHarm, 1, 100, 100);
    mHarm.decompress();
    mHarm.generateNormals();

    // Create parameters
    createInternalTriggerParameter("frequency", 440, 10, 4000.0);
    createInternalTriggerParameter("amplitude", 0.05, 0.0, 1.0);
    createInternalTriggerParameter("attackTime", 0.1, 0.01, 3.0);
    createInternalTriggerParameter("releaseTime", 0.5, 0.1, 10.0);
    createInternalTriggerParameter("sustain", 0.65, 0.1, 1.0);
    createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);

  }

  void onProcess(AudioIOData &io) override
  {
    //Get Parameters
    mOsc.freq(getInternalParameterValue("frequency"));
    mPan.pos(getInternalParameterValue("pan"));
    float amp = getInternalParameterValue("amplitude");
    while (io())
    {
      float s1 = mOsc() * mAmpEnv() * amp;
      float s2;
      mEnvFollow(s1);
      mPan(s1, s1, s2);
      io.out(0) += s1;
      io.out(1) += s2;
    }
    if (mAmpEnv.done() && (mEnvFollow.value() < 0.001))
      free();
  }

  //Replace with obj
  void onProcess(Graphics &g) override
  {
    rotateA += 0.29;
    rotateB += 0.23;
    timepose -= 0.06;
    g.pushMatrix();
    g.depthTesting(true);
    g.lighting(true);
    g.translate(timepose, getInternalParameterValue("frequency") / 200 - 3, -4);
    g.rotate(rotateA, Vec3f(0, 1, 0));
    g.rotate(rotateB, Vec3f(1));
    float scaling = getInternalParameterValue("amplitude") / 10;
    g.scale(scaling + getInternalParameterValue("amplitude") , scaling + getInternalParameterValue("attackTime"), scaling + mEnvFollow.value() * 5);
    g.color(HSV(getInternalParameterValue("amplitude") * 100, getInternalParameterValue("releaseTime") * 20, 0.5 + getInternalParameterValue("pan")));
    g.draw(mHarm);
    g.popMatrix();
  }

   void onTriggerOn() override
  {
    timepose = 10;
    mAmpEnv.reset();

    updateFromParameters();
    
  }

  void onTriggerOff() override
  {
    mAmpEnv.triggerRelease();
  }

  void updateFromParameters()
  {

    mAmpEnv.attack(getInternalParameterValue("attackTime"));
    mAmpEnv.release(getInternalParameterValue("releaseTime"));
    mAmpEnv.sustain(getInternalParameterValue("sustain"));
    
    mPan.pos(getInternalParameterValue("pan"));
  }
  
};

class Melody : public SynthVoice
{
public:
// Unit generators
  gam::Pan<> mPan;
  gam::Saw<> mOsc;
  gam::ADSR<> mAmpEnv;
  // envelope follower to connect audio output to graphics
  gam::EnvFollow<> mEnvFollow;
  // Draw parameters
  Mesh mMel;
  vector<Mesh> melObj;
  double rotateA;
  double rotateB;
  double spin = al::rnd::uniformS();
  double timepose = 0;
  Vec3f note_position;
  Vec3f note_direction;
  Scene *melscene{nullptr};

  void init() override
  {
    mAmpEnv.curve(0); // linear segments
    mAmpEnv.levels(0, 1, 1, 0);
    mAmpEnv.sustainPoint(2);
 
    //Graphics
    std::string melFile = "../cloud_poly.obj"; 
    melscene = Scene::import(melFile); 
    if (!melscene) {
      printf("error reading %s\n", melFile.c_str());
      return;
    }
    melObj.resize(melscene->meshes());
    for (int i = 0; i < melscene->meshes(); i += 1) {
      melscene->mesh(i, melObj[i]);
    }
    //addCube(mMel);
    //mMel.decompress();
    //mMel.generateNormals();

    // Create parameters
    createInternalTriggerParameter("frequency", 440, 10, 4000.0);
    createInternalTriggerParameter("amplitude", 0.05, 0.0, 1.0);
    createInternalTriggerParameter("attackTime", 0.1, 0.01, 3.0);
    createInternalTriggerParameter("releaseTime", 0.5, 0.1, 10.0);
    createInternalTriggerParameter("sustain", 0.65, 0.1, 1.0);
    createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);

  }

  void onProcess(AudioIOData &io) override
  {
    //Get Parameters
    mOsc.freq(getInternalParameterValue("frequency"));
    mPan.pos(getInternalParameterValue("pan"));
    float amp = getInternalParameterValue("amplitude");
    while (io())
    {
      float s1 = mOsc() * mAmpEnv() * amp;
      float s2;
      mEnvFollow(s1);
      mPan(s1, s1, s2);
      io.out(0) += s1;
      io.out(1) += s2;
    }
    if (mAmpEnv.done() && (mEnvFollow.value() < 0.001))
      free();
  }

  void onProcess(Graphics &g) override
  {
    rotateA += 0.29;
    rotateB += 0.23;
    timepose -= 0.06;
    g.pushMatrix();
    g.depthTesting(true);
    g.lighting(true);
    g.translate(timepose, getInternalParameterValue("frequency") / 200 - 3, -4);
    g.rotate(rotateA, Vec3f(0, 1, 0));
    g.rotate(rotateB, Vec3f(1));
    float scaling = getInternalParameterValue("amplitude") / 10;
    g.scale(scaling + getInternalParameterValue("amplitude") , scaling + getInternalParameterValue("attackTime"), scaling + mEnvFollow.value() * 5);
    g.color(HSV(getInternalParameterValue("amplitude") * 20, getInternalParameterValue("releaseTime") * 20, 0.5 + getInternalParameterValue("pan")));
    g.draw(melObj[0]);
    g.popMatrix();
  }

   void onTriggerOn() override
  {
    timepose = 10;
    mAmpEnv.reset();

    updateFromParameters();
    
  }

  void onTriggerOff() override
  {
    mAmpEnv.triggerRelease();
  }

  void updateFromParameters()
  {

    mAmpEnv.attack(getInternalParameterValue("attackTime"));
    mAmpEnv.release(getInternalParameterValue("releaseTime"));
    mAmpEnv.sustain(getInternalParameterValue("sustain"));
    
    mPan.pos(getInternalParameterValue("pan"));
  }


};


class MyApp : public App, public MIDIMessageHandler
{
public:
  SynthGUIManager<Harm> harmManager{"Harm"};
  SynthGUIManager<Melody> melManager{"Melody"}; 
  RtMidiIn midiIn; // MIDI input carrier
  ParameterMIDI parameterMIDI;
  int midiNote;
  float tscale = 1;

  Mesh mSpectrogram;
  vector<float> spectrum;
  bool showGUI = true;
  bool showSpectro = true;
  bool navi = false;
  gam::STFT stft = gam::STFT(FFT_SIZE, FFT_SIZE / 4, 0, gam::HANN, gam::MAG_FREQ);

  //Harmony related
  float timeSinceLastHarm = 0;
  float triggerInterval = 11.0f;

  NoteDecision noteDecision;

  int harmNote1;
  int harmNote2;

  //Melody related
  std::vector<int> currentMelody;
  int currentMelodyIndex = 0;
  float melodyTimeAccum = 0.0f;
  float melodyNoteDuration = 1.0f; 
  int currentMelNote = -1;


  void onInit() override
  {
    imguiInit();

    navControl().active(false); // Disable navigation via keyboard, since we
                                // will be using keyboard for note triggering
    // Set sampling rate for Gamma objects from app's audio
    gam::sampleRate(audioIO().framesPerSecond());
    // Check for connected MIDI devices
    if (midiIn.getPortCount() > 0)
    {
      // Bind ourself to the RtMidiIn object, to have the onMidiMessage()
      // callback called whenever a MIDI message is received
      MIDIMessageHandler::bindTo(midiIn);

      // Open the last device found
      unsigned int port = 0; //midiIn.getPortCount() - 1;
      midiIn.openPort(port);
      printf("Opened port to %s\n", midiIn.getPortName(port).c_str());

      parameterMIDI.open(port, true);
    }
    else
    {
      printf("Error: No MIDI devices found.\n");
    }
    // Declare the size of the spectrum
    spectrum.resize(FFT_SIZE / 2 + 1);


  }

  void onCreate() override
  {
    harmManager.synthRecorder().verbose(true);
    melManager.synthRecorder().verbose(true);
    nav().pos(3, 0, 17);
  }

  void onSound(AudioIOData &io) override
  {
    harmManager.render(io); // Render audio
    melManager.render(io);
    // STFT
    while (io())
    {
      io.out(0) = tanh(io.out(0));
      io.out(1) = tanh(io.out(1));
      if (stft(io.out(0)))
      { // Loop through all the frequency bins
        for (unsigned k = 0; k < stft.numBins(); ++k)
        {
          // Here we simply scale the complex sample
          spectrum[k] = tanh(pow(stft.bin(k).real(), 1.3));
          // spectrum[k] = stft.bin(k).real();
        }
      }
    }
  }

  void onAnimate(double dt) override
  {
    navControl().active(navi); // Disable navigation via keyboard, since we
    imguiBeginFrame();
    harmManager.drawSynthControlPanel();
    melManager.drawSynthControlPanel();
    ParameterGUI::drawParameterMIDI(&parameterMIDI);
    imguiEndFrame();

    //For Gen harm

    timeSinceLastHarm += dt;
    //std::cout << "Time since last harmonic:not working " << timeSinceLastHarm << std::endl;

    if (timeSinceLastHarm >= triggerInterval)
    {
      genHarmOff();  
      genHarmOn(); 
      updateMelody();
      timeSinceLastHarm = 0;
      //std::cout << "Generated Harmonic in if" << std::endl;
    }

    melodyTimeAccum += dt;
    if (!currentMelody.empty() && melodyTimeAccum >= melodyNoteDuration) {
        MelodyOff(); // always turn off the previous one
        currentMelodyIndex = (currentMelodyIndex + 1) % currentMelody.size();
        MelodyOn(); // play the next one
        melodyTimeAccum = 0.0f;
        std::cout <<"Current Melody Note: " << currentMelody[currentMelodyIndex] << std::endl;
    }


  }

  void onDraw(Graphics &g) override
  {
    g.clear();
    harmManager.render(g);
    melManager.render(g);
    // // Draw Spectrum
    mSpectrogram.reset();
    mSpectrogram.primitive(Mesh::LINE_STRIP);
    if (showSpectro)
    {
      for (int i = 0; i < FFT_SIZE / 2; i++)
      {
        mSpectrogram.color(HSV(0.5 - spectrum[i] * 100));
        mSpectrogram.vertex(i, spectrum[i], 0.0);
      }
      g.meshColor(); // Use the color in the mesh
      g.pushMatrix();
      g.translate(-3, -3, 0);
      g.scale(10.0 / FFT_SIZE, 100, 1.0);
      g.draw(mSpectrogram);
      g.popMatrix();
    }
    // GUI is drawn here
    if (showGUI)
    {
      imguiDraw();
    }
  }

  void onMIDIMessage(const MIDIMessage &m)
  {
    switch (m.type())
    {
    case MIDIByte::NOTE_ON:
    {
      int midiNote = m.noteNumber();
      if (midiNote > 0 && m.velocity() > 0.001)
      {
        harmManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
        melManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
        harmManager.voice()->setInternalParameterValue(
            "attackTime", 0.01 / m.velocity());
        melManager.voice()->setInternalParameterValue(
            "attackTime", 0.01 / m.velocity());
        harmManager.triggerOn(midiNote);
        melManager.triggerOn(midiNote);
      }
      else
      {
        harmManager.triggerOff(midiNote);
        melManager.triggerOff(midiNote);
      }
      break;
    }
    case MIDIByte::NOTE_OFF:
    {
      int midiNote = m.noteNumber();
      printf("Note OFF %u, Vel %f", m.noteNumber(), m.velocity());
      harmManager.triggerOff(midiNote);
      melManager.triggerOff(midiNote);
      break;
    }
    default:;
    }
  }

  bool onKeyDown(Keyboard const &k) override
  {
    if (ParameterGUI::usingKeyboard())
    { // Ignore keys if GUI is using them
      return true;
    }
    if (!navi)
    {
      if (k.shift())
      {
        // If shift pressed then keyboard sets preset
        int presetNumber = asciiToIndex(k.key());
        harmManager.recallPreset(presetNumber);
        melManager.recallPreset(presetNumber);
      }
      else
      {
        // Otherwise trigger note for polyphonic synth
        int midiNote = asciiToMIDI(k.key());
        if (midiNote > 0)
        {
          harmManager.voice()->setInternalParameterValue(
              "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
          melManager.voice()->setInternalParameterValue(
              "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
          harmManager.triggerOn(midiNote);
          melManager.triggerOn(midiNote);
        }
      }
    }
    switch (k.key())
    {
    case ']':
      showGUI = !showGUI;
      break;
    case '[':
      showSpectro = !showSpectro;
      break;
    case '-':
      tscale -= 0.1;
      break;
    case '+':
      tscale += 0.1;
      break;
    case '=':
      navi = !navi;
      break;
    }
    return true;
  }

  bool onKeyUp(Keyboard const &k) override
  {
    int midiNote = asciiToMIDI(k.key());
    if (midiNote > 0)
    {
      harmManager.triggerOff(midiNote);
      melManager.triggerOff(midiNote);
    }
    std::cout << "Note OFF: " << midiNote << std::endl;
    return true;
  }



  void genHarmOn() {
    auto chordNotes = noteDecision.getNextHarmonyChord();
    std::random_shuffle(chordNotes.begin(), chordNotes.end());

    for (int i = 0; i < 2; ++i) {
        int midiNote = chordNotes[i];
        float freq = pow(2.f, (midiNote - 69.f) / 12.f) * 432.f;

        // 🎚️ Generate independent random parameters for each voice
        float amp = rnd::uniform(0.1f, 0.6f);
        float attack = rnd::uniform(0.01f, 1.0f);
        float release = rnd::uniform(0.2f, 2.0f);
        float sustain = rnd::uniform(0.4f, 1.0f);
        float pan = rnd::uniformS(); // stereo position: -1.0 (left) to 1.0 (right)

        Harm *voice = harmManager.voice();
        if (!voice) continue;

        //Not 100%sure if these are updating for each note or for each set. 
        voice->setInternalParameterValue("frequency", freq);
        voice->setInternalParameterValue("amplitude", amp);
        voice->setInternalParameterValue("attackTime", attack);
        voice->setInternalParameterValue("releaseTime", release);
        voice->setInternalParameterValue("sustain", sustain);
        voice->setInternalParameterValue("pan", pan);

        harmManager.triggerOn(midiNote);

        if (i == 0) harmNote1 = midiNote;
        if (i == 1) harmNote2 = midiNote;

        // std::cout << "Note " << i << ": " << midiNote
        //           << " freq=" << freq
        //           << " amp=" << amp
        //           << " atk=" << attack
        //           << " rel=" << release
        //           << " sus=" << sustain
        //           << " pan=" << pan << std::endl;
    }
  }

  void genHarmOff()
  {
    harmManager.triggerOff(harmNote1);
    harmManager.triggerOff(harmNote2);
  }

  void MelodyOn() {
      if (currentMelody.empty()) return;

      if (currentMelodyIndex >= currentMelody.size()) {
          currentMelodyIndex = 0; // wrap safely
      }

      int midiNote = currentMelody[currentMelodyIndex];
      currentMelNote = midiNote;

      float freq = pow(2.f, (midiNote - 69.f) / 12.f) * 432.f;

      Melody *voice = melManager.voice();
      if (!voice) return;

      voice->setInternalParameterValue("frequency", freq);
      voice->setInternalParameterValue("amplitude", rnd::uniform(0.1f, 0.4f));
      voice->setInternalParameterValue("attackTime", 0.05f);
      voice->setInternalParameterValue("releaseTime", 0.4f);
      voice->setInternalParameterValue("sustain", 0.7f);
      voice->setInternalParameterValue("pan", rnd::uniformS());

      melManager.triggerOn(midiNote);
  }


  void MelodyOff() {
      if (currentMelNote >= 0) {
          melManager.triggerOff(currentMelNote);
          currentMelNote = -1;
      }
  }



  void updateMelody(){
    if (noteDecision.currentChordIndex < 0 || 
        noteDecision.currentChordIndex >= noteDecision.chordNames.size()) {
        std::cerr << "Invalid currentChordIndex: " << noteDecision.currentChordIndex << std::endl;
        return;
    }

    std::string currentChord = noteDecision.chordNames[noteDecision.currentChordIndex];
    currentMelody = noteDecision.generateLSystemMelody(currentChord);
    currentMelodyIndex = 0;
    melodyTimeAccum = 0.0f;
    currentMelNote = -1;
  }



  void onExit() override { imguiShutdown(); }
  

}; 



int main()
{
  // Create app instance
  MyApp app;

  // Set up audio
  app.configureAudio(48000., 512, 2, 0);
  app.start();
  return 0;
}