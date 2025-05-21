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

using namespace al;
using namespace std;
#define FFT_SIZE 4048


class NoteDecision 
{
  public: 
    
    int currentChordIndex = 0; // Start on "i"

    
    //Self reminder add the 7th note
    std::map<std::string, std::vector<int>> chordDictionary = {
      { "i",    {60, 64, 67, 48, 52, 55} },  // C E G (C4 + C3)
      { "ii",   {62, 66, 69, 50, 54, 57} },  // D F# A  
      { "iii",  {64, 67, 71, 52, 55, 59} },  // E G B
      { "iv",   {66, 69, 72, 54, 57, 60} },  // F# A C
      { "v",    {67, 71, 62, 55, 59, 50} },  // G B D
      { "v64",  {62, 67, 71, 50, 55, 59} },  // D G B
      { "vi",   {69, 72, 76, 57, 60, 64} },  // A C E
      { "vii",  {71, 74, 69, 59, 62, 57} }   // B D A
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
    g.scale(scaling + getInternalParameterValue("amplitude") / 10, scaling + getInternalParameterValue("attackTime") / 30, scaling + mEnvFollow.value() * 5);
    g.color(HSV(getInternalParameterValue("amplitude") / 20, getInternalParameterValue("releaseTime") / 20, 0.5 + getInternalParameterValue("pan")));
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


class MyApp : public App, public MIDIMessageHandler
{
public:
  SynthGUIManager<Harm> synthManager{"Harm"};
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

  float timeSinceLastHarm = 0;
  float triggerInterval = 2.0f;

  NoteDecision noteDecision;

  int harmNote1;
  int harmNote2;

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
    synthManager.synthRecorder().verbose(true);
    nav().pos(3, 0, 17);
  }

  void onSound(AudioIOData &io) override
  {
    synthManager.render(io); // Render audio
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
    synthManager.drawSynthControlPanel();
    ParameterGUI::drawParameterMIDI(&parameterMIDI);
    imguiEndFrame();

    //For Gen harm

    timeSinceLastHarm += dt;
    //std::cout << "Time since last harmonic:not working " << timeSinceLastHarm << std::endl;

    if (timeSinceLastHarm >= triggerInterval)
    {
      genHarmOff();
      genHarmOn();
      timeSinceLastHarm = 0;
      std::cout << "Generated Harmonic in if" << std::endl;
    }

  }

  void onDraw(Graphics &g) override
  {
    g.clear();
    synthManager.render(g);
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
        synthManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
        synthManager.voice()->setInternalParameterValue(
            "attackTime", 0.01 / m.velocity());
        synthManager.triggerOn(midiNote);
      }
      else
      {
        synthManager.triggerOff(midiNote);
      }
      break;
    }
    case MIDIByte::NOTE_OFF:
    {
      int midiNote = m.noteNumber();
      printf("Note OFF %u, Vel %f", m.noteNumber(), m.velocity());
      synthManager.triggerOff(midiNote);
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
        synthManager.recallPreset(presetNumber);
      }
      else
      {
        // Otherwise trigger note for polyphonic synth
        int midiNote = asciiToMIDI(k.key());
        if (midiNote > 0)
        {
          synthManager.voice()->setInternalParameterValue(
              "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
          synthManager.triggerOn(midiNote);
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
      synthManager.triggerOff(midiNote);
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

        Harm *voice = synthManager.voice();
        if (!voice) continue;

        //Not 100%sure if these are updating for each note or for each set. 
        voice->setInternalParameterValue("frequency", freq);
        voice->setInternalParameterValue("amplitude", amp);
        voice->setInternalParameterValue("attackTime", attack);
        voice->setInternalParameterValue("releaseTime", release);
        voice->setInternalParameterValue("sustain", sustain);
        voice->setInternalParameterValue("pan", pan);

        synthManager.triggerOn(midiNote);

        if (i == 0) harmNote1 = midiNote;
        if (i == 1) harmNote2 = midiNote;

        std::cout << "Note " << i << ": " << midiNote
                  << " freq=" << freq
                  << " amp=" << amp
                  << " atk=" << attack
                  << " rel=" << release
                  << " sus=" << sustain
                  << " pan=" << pan << std::endl;
    }
  }


  // void genHarmOn()
  // {
    
  //   auto harmony = noteDecision.getNextHarmonyChord();
  //   std::cout << "Harmony: " << harmony[0] << " " << harmony[1] << std::endl;
    
  //   harmNote1 = rnd::uniform(60, 72); // C3 to C5
  //   float freq = ::pow(2.f, (harmNote1 - 69.f) / 12.f) * 432.f; // You can also use 440.f
    
  //   Harm *voice1 = synthManager.voice(); 
  //   voice1->setInternalParameterValue("frequency", freq);
  //   voice1->setInternalParameterValue("amplitude", 0.1);
  //   voice1->setInternalParameterValue("attackTime", 0.1);
  //   voice1->setInternalParameterValue("releaseTime", 0.1);
  //   voice1->setInternalParameterValue("sustain", 0);
  //   voice1->setInternalParameterValue("pan", rnd::uniformS());
  //   synthManager.triggerOn(harmNote1);

  //   harmNote2 = rnd::uniform(24, 36);
  //   float freq2 = ::pow(2.f, (harmNote2 - 69.f) / 12.f) * 432.f; // You can also use 440.f

  //   Harm *voice2 = synthManager.voice(); 
  //   voice2->setInternalParameterValue("frequency", freq2);
  //   voice2->setInternalParameterValue("amplitude", 0.1);
  //   voice2->setInternalParameterValue("attackTime", 0.1);
  //   voice2->setInternalParameterValue("releaseTime", 0.1);
  //   voice2->setInternalParameterValue("sustain", 1);
  //   voice2->setInternalParameterValue("pan", rnd::uniformS());
  //   synthManager.triggerOn(harmNote2);
    
  // }

  void genHarmOff()
  {
    synthManager.triggerOff(harmNote1);
    synthManager.triggerOff(harmNote2);
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