#pragma once
#include "PluginProcessor.h"

class PedalView;

class WharryEditor : public juce::AudioProcessorEditor
{
public:
    explicit WharryEditor (WharryProcessor&);
    ~WharryEditor() override;
    void resized() override;
    void parentHierarchyChanged() override;
    int  getScaleIndex() const;          // 1, 2 = 75 %, 100 %  (-1 = taille par défaut / libre)
    void setScaleIndex (int idx);        // -1 = taille par défaut (67 %, aussi la taille minimale)

private:
    friend class PedalView;              // son timer relance useSoftwareRenderer()
    void useSoftwareRenderer();
    WharryProcessor& proc;
    std::unique_ptr<PedalView> view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WharryEditor)
};
