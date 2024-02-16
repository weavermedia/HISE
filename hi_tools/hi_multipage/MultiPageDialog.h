/*  ===========================================================================
 *
 *   This file is part of HISE.
 *   Copyright 2016 Christoph Hart
 *
 *   HISE is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   HISE is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Commercial licenses for using HISE in an closed source project are
 *   available on request. Please visit the project's website to get more
 *   information about commercial licensing:
 *
 *   http://www.hise.audio/
 *
 *   HISE is based on the JUCE library,
 *   which must be separately licensed for closed source applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

#pragma once

namespace hise {
namespace multipage {
using namespace juce;



namespace factory
{
	struct Container;
}

struct ErrorComponent: public Component
{
    ErrorComponent(Dialog& parent_);;
    
    void paint(Graphics& g) override;
    void resized() override;

    void setError(const Result& r);
    void show(bool shouldShow);
    
    Dialog& parent;

    int height = 40;
    MarkdownRenderer parser;
    ScopedPointer<HiseShapeButton> closeButton;
    Result currentError;
};


/* TODO:
 *
 * - make dummy app (with exporter in HISE?)
 * - remove nested data when using IDs in containers (too messy)...
 * - fix undoability of page removal
 * - add fixed size to position info
 * */
class Dialog: public Component,
			  public PathFactory,
			  public ScrollBar::Listener
{
public:
    
    enum ColourIds
    {
        backgroundColour = HiseColourScheme::ComponentBackgroundColour,
        textColour = HiseColourScheme::ComponentTextColourId,
        signalColour = HiseColourScheme::ComponentFillTopColourId,
        numColourIds
    };


    struct PageInfo;

    struct PositionInfo
    {
        enum LabelPositioning
	    {
	        Default,
		    Left,
	        Above,
	        None,
            numLabelPositionings
	    };

        static StringArray getLabelPositionNames()
        {
	        return { "Default", "Left", "Above", "None" };
        }

        var toJSON() const;
        void fromJSON(const var& obj);
        Result checkSanity() const;
        void setDefaultPosition(LabelPositioning p);
        int getWidthForLabel(int totalWidth) const;
        int getHeightForComponent(int heightWithoutLabel);

        int TopHeight = 56;
        int ButtonTab = 40;
        int ButtonMargin = 5;
        int OuterPadding = 50;
        double LabelWidth = 160;
        int LabelHeight = 32;
        int LabelPosition = LabelPositioning::Default;
    };

    struct PageBase: public Component
    {
        using CustomCheckFunction = std::function<Result(PageBase*, var)>;

        PageBase(Dialog& rootDialog, int width, const var& obj);
        virtual ~PageBase() {};

        virtual void postInit() {};
        virtual DefaultProperties getDefaultProperties() const;
        virtual void setStateObject(const var& newStateObject);
        virtual void clearInitValue()
        {
	        initValue = var();
        }

        virtual void createEditor(PageInfo& infoList) {}

        bool matches(const var& otherInfo) const
        {
	        return infoObject.getDynamicObject() == otherInfo.getDynamicObject();
        }

        var getPropertyFromInfoObject(const Identifier& id) const;
        void deleteFromParent();
        void duplicateInParent();
        bool isEditModeAndNotInPopup() const;
        bool showDeletePopup(bool isRightClick);
        String getDefaultPositionName() const;

        /** Returns the string from the info object for the given property. If it contains a wildcard with "$", it will resolve it using the state object. */
        String evaluate(const Identifier& id) const;

    protected:

        virtual Result checkGlobalState(var globalState) = 0;
        void init();

#if HISE_MULTIPAGE_INCLUDE_EDIT
        ScopedPointer<EditorOverlay> overlay;
#endif
        virtual void editModeChanged(bool isEditMode);
        
	    bool inheritsPosition = true;

    public:

        template <typename PageSubType, typename StopType=juce::Toolbar> PageSubType* getRootPage()
        {
            auto p = this;
            
            while(auto otherP = p->findParentComponentOfClass<PageSubType>())
            {
                if(dynamic_cast<StopType*>(otherP) != nullptr)
                    return dynamic_cast<PageSubType*>(p);
                
                p = otherP;
            }
            
            return dynamic_cast<PageSubType*>(p);
        }
        
        void writeState(const var& newValue) const;
        var getValueFromGlobalState(var defaultState=var());
        Result check(const var& obj);
        void clearCustomFunction();
        void setCustomCheckFunction(const CustomCheckFunction& cf_);
        bool isError() const;
        Dialog& getParentDialog() { return rootDialog; }

        Identifier getId() const { return id; }

    protected:

        PositionInfo positionInfo;

        Identifier id;
        Dialog& rootDialog;
        var initValue;

        struct HelpButton: public HiseShapeButton
	    {
		    HelpButton(String help, const PathFactory& factory);
	    };

        ScopedPointer<HelpButton> helpButton;
        CustomCheckFunction cf;
        
        var stateObject;
        var infoObject;

        void setModalHelp(const String& text);

        enum class AreaType
	    {
		    Label,
	        Component
	    };

	    Rectangle<int> getArea(AreaType t) const;

    private:

        static void onEditModeChange(PageBase& c, bool isOn);

        struct ModalHelp;

        ScopedPointer<Component> modalHelp;

        JUCE_DECLARE_WEAK_REFERENCEABLE(PageBase);
    };
    
    struct PageInfo: public ReferenceCountedObject
    {
        using Ptr = ReferenceCountedObjectPtr<PageInfo>;
        using List = ReferenceCountedArray<PageInfo>;
        using CreateFunction = std::function<PageBase*(Dialog&, int width, const var&)>; 

        PageInfo() = default;
        PageInfo(const var& obj): data(obj) {};

        operator bool() const noexcept { return (bool)pageCreator; }
        PageBase* create(Dialog& r, int currentWidth) const;;
        var& operator[](const Identifier& id) const;
        var getData() const { return data; }
        
        template <typename T> static PageInfo::Ptr createInfo()
        {
	        PageInfo::Ptr p = new PageInfo();
	        p->data = new DynamicObject();
	        p->data.getDynamicObject()->setProperty(mpid::Type, T::getStaticId().toString());
	        p->pageCreator = [](Dialog& r, int w, const var& d){ return new T(r, w, d); };
            return p;
        }

        PageInfo* getChildRecursive(PageInfo* p, const Identifier& id);

        PageInfo& getChild(const Identifier& id);

        template <typename T> PageInfo& addChild(DefaultProperties&& values={})
        {
            static_assert(std::is_base_of<PageBase, T>(), "must be base of PageBase");
	        childItems.add(createInfo<T>());
            
            for(auto& v: values)
                childItems.getLast()->data.getDynamicObject()->setProperty(v.first, v.second);
            
            return *childItems.getLast();
        }

        void setLambdaAction(State& s, const State::HardcodedLambda& f)
        {
	        (*this)[mpid::Function] = var(LambdaAction(s, f));
        }

        void setCreateFunction(const CreateFunction& f);
        void setCustomCheckFunction(const PageBase::CustomCheckFunction& f);
        void setStateObject(const var& newStateObject);
        
        bool useGlobalStateObject = false;
        var stateObject;
        var data;
        CreateFunction pageCreator;
        List childItems;
        PageBase::CustomCheckFunction customCheck;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PageInfo);
    };

    Dialog(const var& obj, State& rt, bool addEmptyPage=true);
    ~Dialog();

    int getNumPages() const { return pages.size(); }
    bool removeCurrentPage();
    void addListPageWithJSON();

    template <typename T> PageInfo& addPage(DefaultProperties&& values={}, int index = -1)
    {
        PageInfo::Ptr p;

        if(index == -1)
			p = pages.add(PageInfo::createInfo<T>());
        else
            p = pages.insert(index, PageInfo::createInfo<T>());
        
        if(values.size())
        {
            for (const auto& v: values)
                (*p)[v.first] = v.second;
        }
        else
        {
            for (const auto& v: T::getStaticDefaultProperties())
                (*p)[v.first] = v.second;
        }
        
        return *p;
    }
    
    Result getCurrentResult();
    void showFirstPage();
	void setFinishCallback(const std::function<void()>& f);
    static var getOrCreateChild(const var& obj, const Identifier& id);
    static var getGlobalState(Component& page, const Identifier& id, const var& defaultValue);
    static std::pair<Font, Colour> getDefaultFont(Component& c);
    State::Job::Ptr getJob(const var& obj) const;
    Path createPath(const String& url) const override;
    MarkdownLayout::StyleData getStyleData() const { return styleData; }
    void setProperty(const Identifier& id, const var& newValue);
    void setStyleData(const MarkdownLayout::StyleData& sd);
    bool navigate(bool forward);

    String getStringFromModalInput(const String& message, const String& prefilledValue);

    bool refreshCurrentPage();

    bool getCurrentNavigationDirection() { return currentNavigation; }
    void paint(Graphics& g) override;
    void resized() override;
    State& getState() { return *runThread; }

    struct TabTraverser;

    std::unique_ptr<ComponentTraverser> createKeyboardFocusTraverser() override;

    void setEditMode(bool isEditMode);

    bool isEditModeEnabled() const noexcept;

    bool isEditModeAllowed() const { return editingAllowed; }

    String getExistingKeysAsItemString() const;

    void setEnableEditMode(bool shouldBeEnabled)
    {
	    editingAllowed = shouldBeEnabled;
    }

    void containerPopup(const var& infoObject);

    bool nonContainerPopup(const var& infoObject);

    bool showEditor(const var& infoObject);

    void gotoPage(int newIndex);

    LambdaBroadcaster<bool>& getEditModeBroadcaster() { return editModeBroadcaster; }

    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() {};
        virtual void drawMultiPageHeader(Graphics& g, Dialog& d, Rectangle<int> area);
        virtual void drawMultiPageButtonTab(Graphics& g, Dialog& d, Rectangle<int> area);
        virtual void drawMultiPageModalBackground(Graphics& g, Rectangle<int> totalBounds, Rectangle<int> modalBounds);
        virtual void drawMultiPageFoldHeader(Graphics& g, Component& c, Rectangle<float> area, const String& title, bool folded);
        virtual void drawMultiPageBackground(Graphics& g, Dialog& tb, Rectangle<int> errorBounds);
        virtual PositionInfo getMultiPagePositionInfo(const var& pageData) const = 0;
    };
    
    struct DefaultLookAndFeel: public hise::GlobalHiseLookAndFeel,
                               public LookAndFeelMethods
    {
        void drawToggleButton(Graphics& g, ToggleButton& tb, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        PositionInfo getMultiPagePositionInfo(const var& pageData) const override;
        void layoutFilenameComponent (FilenameComponent& filenameComp, ComboBox* filenameBox, Button* browseButton) override;
        bool isProgressBarOpaque(ProgressBar&) override { return false; }
        void drawProgressBar (Graphics &g, ProgressBar &pb, int width, int height, double progress, const String &textToShow) override;

        PositionInfo defaultPosition;
    } defaultLaf;

    struct ModalPopup: public Component
    {
        ModalPopup(Dialog& parent_);;

        void onOk();
        void dismiss();
        void setContent(PageInfo::Ptr newContent);
        void show(PositionInfo newInfo, bool addButtons, const Image& additionalScreenshot);

        bool keyPressed(const KeyPress& k) override;

        void mouseDown(const MouseEvent& e) override;
        void paint(Graphics& g) override;
        void resized() override;

        Dialog& parent;
        PositionInfo info;
        ErrorComponent modalError;
        Rectangle<int> modalBounds;
        ScopedPointer<PageBase> contentComponent;
        PageInfo::Ptr content;
	    TextButton okButton, cancelButton;

        Image screenshot;
    };

    template <typename T> PageInfo& createModalPopup(DefaultProperties&& values={})
    {
	    auto p = PageInfo::createInfo<T>();
        
        if(values.size())
        {
            for (const auto& v: values)
                (*p)[v.first] = v.second;
        }
        else
        {
            for (const auto& v: T::getStaticDefaultProperties())
                (*p)[v.first] = v.second;
        }

        popup.setContent(p);
        return *p;
    }

    static String joinVarArrayToNewLineString(const var& v);

    static var parseCommaList(const String& text);

    void showModalPopup(bool addButtons, const Image& additionalScreenshot = {});
    Result checkCurrentPage();
    void setCurrentErrorPage(PageBase* b);
    bool keyPressed(const KeyPress& k) override;
    Viewport& getViewport() { return content; }
    var exportAsJSON() const;
    void scrollBarMoved (ScrollBar*, double) override { repaint(); }

    PageBase* findPageBaseForInfoObject(const var& obj);

    PositionInfo getPositionInfo(const var& pageData) const;

    bool createJSON = false;

    UndoManager& getUndoManager() { return um; }

    var getPageListVar() { return pageListArrayAsVar; }

    LambdaBroadcaster<int> refreshBroadcaster;

private:



    void showMainPropertyEditor();

    UndoManager um;

    TooltipWindow tooltipWindow;

    LambdaBroadcaster<bool> editModeBroadcaster;

    bool editMode = false;
    bool editingAllowed = true;

    friend class State;

    Array<var>* pageListInfo;

    var pageListArrayAsVar;

    var assets;
    bool currentNavigation = true;
    var properties;
    MarkdownLayout::StyleData styleData;
    PageInfo::List pages;
    TextButton cancelButton;
    TextButton nextButton;
    TextButton prevButton;
    State* runThread;
    Viewport content;
    ScopedPointer<PageBase> currentPage;
    Rectangle<int> top, bottom, center;
    std::function<void()> finishCallback;
    
    WeakReference<PageBase> currentErrorElement;
    ErrorComponent errorComponent;
    ModalPopup popup;
    ScrollbarFader sf;
    
#if HISE_MULTIPAGE_INCLUDE_EDIT
    EditorOverlay mainPropertyOverlay;
#endif

    JUCE_DECLARE_WEAK_REFERENCEABLE(Dialog);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Dialog);
};

}
}