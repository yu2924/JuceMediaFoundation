//
//  MainWindow.cpp
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#include "MainWindow.h"
#include "SettingsWindow.h"

// ================================================================================
// WaveformView

class WaveformView
	: public juce::Component
	, public juce::ChangeListener
{
protected:
	class CursorTip
		: public juce::Component
	{
	public:
		CursorTip()
		{
			setOpaque(true);
		}
		virtual void paint(juce::Graphics& g) override
		{
			g.fillAll(juce::Colour(0xffff7f0e));
		}
	};
	juce::AudioThumbnail thumbnail;
	CursorTip cursorTip;
	double duration = 0;
	double cursorPosition = 0;
	enum { XMargin = 8, YMargin = 4, };
public:
	std::function<void(double)> onMouseDrag;
	WaveformView(juce::AudioFormatManager& afm, juce::AudioThumbnailCache& atc)
		: thumbnail(128, afm, atc)
	{
		setOpaque(true);
		addChildComponent(cursorTip);
		cursorTip.setBounds(calcCursorRect(cursorPosition));
		thumbnail.addChangeListener(this);
	}
	virtual ~WaveformView()
	{
		thumbnail.removeChangeListener(this);
	}
	juce::Rectangle<int> calcCursorRect(double t) const
	{
		juce::Rectangle<int> rcwf = getLocalBounds().reduced(XMargin, YMargin);
		int x = (0 < duration) ? juce::roundToInt((double)rcwf.getX() + (double)rcwf.getWidth() * t / duration) : 0;
		return { x, rcwf.getY(), 1, rcwf.getHeight() };
	}
	virtual void resized() override
	{
		cursorTip.setBounds(calcCursorRect(cursorPosition));
	}
	virtual void paint(juce::Graphics& g) override
	{
		juce::Rectangle<int> rcwf = getLocalBounds().reduced(XMargin, YMargin);
		juce::Rectangle<int> rcclip = g.getClipBounds();
		juce::Rectangle<int> rcx = rcwf.getIntersection(rcclip);
		juce::Rectangle<int> rcd = { rcx.getX(), rcwf.getY(), rcx.getWidth(), rcwf.getHeight() };
		g.setColour(juce::Colour(0xff202020));
		g.fillRect(rcclip);
		if(!rcd.isEmpty())
		{
			int xb = rcwf.getX();
			int cx = rcwf.getWidth();
			int x0 = rcd.getX();
			int x1 = rcd.getRight();
			double t0 = (double)(x0 - xb) / (double)cx * duration;
			double t1 = (double)(x1 - xb) / (double)cx * duration;
			g.setColour(juce::Colour(0xff20a685));
			thumbnail.drawChannels(g, rcd, t0, t1, 1);
		}
	}
	virtual void mouseDown(const juce::MouseEvent& me) override
	{
		if(onMouseDrag) onMouseDrag((double)(me.x - XMargin) / (double)(getWidth() - XMargin * 2) * duration);
	}
	virtual void mouseDrag(const juce::MouseEvent& me) override
	{
		if(onMouseDrag) onMouseDrag((double)(me.x - XMargin) / (double)(getWidth() - XMargin * 2) * duration);
	}
	virtual void changeListenerCallback(juce::ChangeBroadcaster* source) override
	{
		if(source == &thumbnail) repaint();
	}
	void setAudioFile(const juce::File& v)
	{
		duration = 0;
		cursorPosition = 0;
		thumbnail.setSource(nullptr);
		if(v != juce::File())
		{
			thumbnail.setSource(new juce::FileInputSource(v));
			duration = thumbnail.getTotalLength();
		}
		cursorTip.setVisible(0 < duration);
		trackCursorPosition(0, false);
	}
	void trackCursorPosition(double t, bool playing)
	{
		cursorPosition = t;
		juce::ComponentAnimator& anim = juce::Desktop::getInstance().getAnimator();
		anim.cancelAnimation(&cursorTip, false);
		cursorTip.setBounds(calcCursorRect(cursorPosition));
		if((0 < duration) && playing)
		{
			juce::Rectangle<int> rce = calcCursorRect(duration);
			int tms = juce::roundToInt((duration - cursorPosition) * 1000);
			anim.animateComponent(&cursorTip, rce, 1, tms, false, 1, 1);
		}
	}
};

// ================================================================================
// MainComponent

class MainComponent
	: public juce::Component
	, public juce::FileDragAndDropTarget
	, public juce::ChangeListener
	, public juce::Timer
{
private:
	juce::AudioDeviceManager& audioDeviceManager;
	juce::AudioFormatManager& audioFormatManager;
	AudioFilePlayer& audioFilePlayer;
	juce::TextButton rewindButton;
	juce::TextButton playButton;
	juce::Label fileLabel;
	juce::Label formatLabel;
	juce::TextButton fileButton;
	juce::TextButton settingsButton;
	WaveformView waveformView;
	juce::Component::SafePointer<SettingsWindow> settingsWindow;
	juce::File audioFile;
	enum { Margin = 8, Spacing = 4, ControlHeight = 24, ButtonUnitWidth = 32, FormatWidth = 128 };
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
public:
	MainComponent(juce::AudioDeviceManager& adm, juce::AudioFormatManager& afm, juce::AudioThumbnailCache& atc, AudioFilePlayer& afp)
		: audioDeviceManager(adm)
		, audioFormatManager(afm)
		, audioFilePlayer(afp)
		, waveformView(afm, atc)
	{
		addAndMakeVisible(rewindButton);
		rewindButton.setButtonText("<<");
		rewindButton.setTooltip("rewind");
		rewindButton.addShortcut(juce::KeyPress('w'));
		rewindButton.onClick = [this]()
		{
			audioFilePlayer.setPlaybackPosition(0);
		};
		addAndMakeVisible(playButton);
		playButton.setButtonText(">");
		playButton.setTooltip("stop|play");
		playButton.setColour(juce::TextButton::ColourIds::buttonOnColourId, juce::Colours::limegreen);
		playButton.addShortcut(juce::KeyPress(juce::KeyPress::spaceKey));
		playButton.onClick = [this]()
		{
			audioFilePlayer.setRunning(!audioFilePlayer.isRunning());
		};
		addAndMakeVisible(fileLabel);
		fileLabel.setColour(juce::Label::ColourIds::outlineColourId, findColour(juce::ComboBox::ColourIds::outlineColourId));
		addAndMakeVisible(formatLabel);
		addAndMakeVisible(fileButton);
		fileButton.setButtonText("...");
		fileButton.setTooltip("choose an audio file");
		fileButton.setConnectedEdges(juce::Button::ConnectedEdgeFlags::ConnectedOnLeft);
		fileButton.onClick = [this]()
		{
			std::shared_ptr<juce::FileChooser> dlg = std::make_shared<juce::FileChooser>("Open", juce::File(), "*.*");
			using FCF = juce::FileBrowserComponent::FileChooserFlags;
			dlg->launchAsync(FCF::openMode | FCF::canSelectFiles, [this, dlg](const juce::FileChooser& fc)
			{
				juce::File f = fc.getResult();
				if(f != juce::File()) openAudioFile(f);
			});
		};
		addAndMakeVisible(settingsButton);
		settingsButton.setButtonText("Device");
		settingsButton.setTooltip("setup audio device environment");
		settingsButton.onClick = [this]()
		{
			if(!settingsWindow) settingsWindow = SettingsWindow::createInstance(audioDeviceManager);
			settingsWindow->toFront(true);
		};
		addAndMakeVisible(waveformView);
		waveformView.onMouseDrag = [this](double t)
		{
			audioFilePlayer.setPlaybackPosition(t);
		};
		setSize(800, 320);
		audioFilePlayer.addChangeListener(this);
	}
	virtual ~MainComponent() override
	{
		audioFilePlayer.removeChangeListener(this);
		if(settingsWindow) settingsWindow->closeWindow();
	}
	void openAudioFile(const juce::File& v)
	{
		audioFile = v;
		audioFilePlayer.setAudioFile(audioFile);
		waveformView.setAudioFile(audioFilePlayer.getAudioFile());
		fileLabel.setText(audioFile.getFileName(), juce::NotificationType::dontSendNotification);
		fileLabel.setTooltip(audioFile.getFullPathName());
		formatLabel.setText(audioFilePlayer.getFormatName(), juce::NotificationType::dontSendNotification);
	}
	void reopenAudioFile()
	{
		openAudioFile(audioFile);
	}
	virtual void resized() override
	{
		juce::Rectangle<int> rc = getLocalBounds().reduced(Margin);
		juce::Rectangle<int> rcc = rc.removeFromTop(ControlHeight);
		rewindButton.setBounds(rcc.removeFromLeft(ButtonUnitWidth));
		playButton.setBounds(rcc.removeFromLeft(ButtonUnitWidth));
		rcc.removeFromLeft(Spacing);
		settingsButton.setBounds(rcc.removeFromRight(ButtonUnitWidth * 2));
		rcc.removeFromRight(Spacing);
		formatLabel.setBounds(rcc.removeFromRight(FormatWidth));
		fileButton.setBounds(rcc.removeFromRight(rcc.getHeight()));
		fileLabel.setBounds(rcc);
		rc.removeFromTop(Spacing);
		waveformView.setBounds(rc);
	}
	virtual void paint(juce::Graphics& g) override
	{
		g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
	}
	virtual bool isInterestedInFileDrag(const juce::StringArray& files) override
	{
		return files.size() == 1;
	}
	virtual void filesDropped(const juce::StringArray & files, int, int) override
	{
		if(files.size() == 1) openAudioFile(files[0]);
	}
	virtual void changeListenerCallback(juce::ChangeBroadcaster* source) override
	{
		if(source == &audioFilePlayer)
		{
			double t = audioFilePlayer.getPlaybackPosition();
			bool running = audioFilePlayer.isRunning();
			waveformView.trackCursorPosition(t, running);
			playButton.setToggleState(running, juce::NotificationType::dontSendNotification);
			if(running && !isTimerRunning()) startTimer(100);
			if(!running && isTimerRunning()) stopTimer();
		}
	}
	virtual void timerCallback() override
	{
		double t = audioFilePlayer.getPlaybackPosition();
		bool running = audioFilePlayer.isRunning();
		waveformView.trackCursorPosition(t, running);
	}
};

// ================================================================================
// MainWindow

class MainWindowImpl
	: public MainWindow
{
private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindowImpl)
public:
	MainWindowImpl(juce::AudioDeviceManager& adm, juce::AudioFormatManager& afm, juce::AudioThumbnailCache& atc, AudioFilePlayer& afp)
		: MainWindow(juce::JUCEApplication::getInstance()->getApplicationName(), juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId), DocumentWindow::allButtons, true)
	{
		setUsingNativeTitleBar(true);
		setResizable(true, true);
		setContentOwned(new MainComponent(adm, afm, atc, afp), true);
		centreWithSize(getWidth(), getHeight());
		setVisible(true);
	}
	virtual void closeButtonPressed() override
	{
		juce::JUCEApplication::getInstance()->systemRequestedQuit();
	}
};

std::unique_ptr<MainWindow> MainWindow::createInstance(juce::AudioDeviceManager& adm, juce::AudioFormatManager& afm, juce::AudioThumbnailCache& atc, AudioFilePlayer& afp)
{
	return std::make_unique<MainWindowImpl>(adm, afm, atc, afp);
}

