#pragma once

#include <atomic>

namespace Q3Win8
{
    struct MSG;
}

ref class Quake3Win8App sealed : public Windows::ApplicationModel::Core::IFrameworkView
{
public:
	Quake3Win8App();
	
	// IFrameworkView Methods.
	virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
	virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
	virtual void Load(Platform::String^ entryPoint);
	virtual void Run();
	virtual void Uninitialize();

protected:
	// Event Handlers.
	void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args);
	void OnLogicalDpiChanged(Platform::Object^ sender);
	void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);
	void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args);
	void OnResuming(Platform::Object^ sender, Platform::Object^ args);
	void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);
	void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
	void OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
	void OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
	void OnPointerMoved(Windows::Devices::Input::MouseDevice^ sender, Windows::Devices::Input::MouseEventArgs^ args);
	void OnPointerWheelChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
    void OnCharacterReceived(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CharacterReceivedEventArgs^ args);
    void OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
    void OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);


private:

    void HandleExceptionMessage( const Q3Win8::MSG* msg );
    void WaitForGameReady();
    void HandleMessagesFromGame();
    void HandleMessageFromGame( const Q3Win8::MSG* msg );
    void CaptureMouse();
    void ReleaseMouse();

    std::atomic<bool> m_windowClosed;
	std::atomic<bool> m_windowVisible;
    Windows::Foundation::Size m_logicalSize;
    Concurrency::task<void> m_gameThread;
    std::atomic<bool> m_gameIsDone;
    
    Windows::Foundation::EventRegistrationToken m_mouseCaptureHandle;
    Windows::Foundation::IAsyncOperation<Windows::UI::Popups::IUICommand^>^ m_currentMsgDlg;
};

ref class Quake3Win8ApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView();
};

void Win8_InitTimer();
int Win8_GetTime();
size_t Win8_CopyString( Platform::String^ str, char* dst, size_t dstLen );
Platform::String^ Win8_CopyString( const char* src );
size_t Win8_MultiByteToWide( const char* src, wchar_t* dst, size_t dstLen );
void Win8_SetCommandLine( Platform::Array<Platform::String^>^ args );
void Win8_Throw( HRESULT hr, Platform::String^ str );
Windows::Foundation::IAsyncOperation<Windows::UI::Popups::IUICommand^>^ Win8_DisplayException( Platform::Exception^ ex );
void Win8_SetCommandLine( Platform::Array<Platform::String^>^ args );
void Win8_PostQuitMessage();

template<class Type> IUnknown* Win8_GetPointer( Type^ obj )
{
    Platform::Agile<Type> agile( obj );
    IUnknown* ptr = reinterpret_cast<IUnknown*>( agile.Get() );
    ptr->AddRef();
    return ptr;
}

template<class Type> Type^ Win8_GetType( IUnknown* obj )
{
    Platform::Details::AgileHelper<Type> agile( reinterpret_cast<__abi_IUnknown*>( obj ), true );
    return agile;
}



// @pjb: 
void Sys_SetFrameTime( int time );
