#include "common_utils.h"
#include "my_webview.h"
#include <filesystem>
#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <wrl/client.h>
#include <WebView2EnvironmentOptions.h>

using namespace Microsoft::WRL;

std::wstring ReadHtmlFile(const std::wstring &filePath)
{
    std::wifstream file(filePath);
    if (!file)
    {
        // TODO: Log
        return L"";
    }
    // Use Boost Locale to handle UTF-8
    file.imbue(boost::locale::generator().generate("en_US.UTF-8"));
    std::wstringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int PrepareWindowHtml()
{
    std::wstring htmlPath =
        std::filesystem::current_path().wstring() + L"/html/index.html";
    ::HTMLString = ReadHtmlFile(htmlPath);
    std::wstring bodyPath =
        std::filesystem::current_path().wstring() + L"/html/body.html";
    ::BodyString = ReadHtmlFile(bodyPath);
    return 0;
}

void UpdateHtmlContentWithJavaScript(ComPtr<ICoreWebView2> webview,
                                     const std::wstring &newContent)
{
    if (webview != nullptr)
    {
        std::wstring script =
            L"document.body.innerHTML = `" + newContent + L"`;";
        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

// Handle WebView2 controller creation
HRESULT OnControllerCreated(            //
    HWND hWnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        ShowErrorMessage(hWnd, L"Failed to create WebView2 controller.");
        return E_FAIL;
    }

    webviewController = controller;
    webviewController->get_CoreWebView2(webview.GetAddressOf());

    if (!webview)
    {
        ShowErrorMessage(hWnd, L"Failed to get WebView2 instance.");
        return E_FAIL;
    }

    // Configure WebView settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webview->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
    }

    // Configure virtual host path
    if (SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&webview3))))
    {
        webview3->SetVirtualHostNameToFolderMapping(         //
            L"appassets",                                    //
            ::LocalAssetsPath.c_str(),                       //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS //
        );                                                   //
    }

    // Set transparent background
    if (SUCCEEDED(
            controller->QueryInterface(IID_PPV_ARGS(&webviewController2))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hWnd, &bounds);
    webviewController->put_Bounds(bounds);

    std::wstring htmlPath =
        std::filesystem::current_path().wstring() + L"/html/index.html";
    std::wstring fileUri = L"file:///" + htmlPath;
    // Navigate to HTML
    HRESULT hr = webview->Navigate(fileUri.c_str());
    if (FAILED(hr))
    {
        ShowErrorMessage(hWnd, L"Failed to navigate to index.html.");
    }

    // webview->OpenDevToolsWindow();

    return S_OK;
}

// Handle WebView2 environment creation
HRESULT OnEnvironmentCreated(HWND hWnd, HRESULT result,
                             ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
        ShowErrorMessage(hWnd, L"Failed to create WebView2 environment.");
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                //
        hWnd,                                                                //
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [hWnd](HRESULT result,                                           //
                   ICoreWebView2Controller *controller) -> HRESULT {         //
                return OnControllerCreated(hWnd, result, controller);        //
            })                                                               //
            .Get()                                                           //
    );                                                                       //
}

// Initialize WebView2
void InitWebview(HWND hWnd)
{
    // Create environment options
    Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions> options;
    Microsoft::WRL::MakeAndInitialize<CoreWebView2EnvironmentOptions>(&options);
    options->put_AdditionalBrowserArguments(L"--allow-file-access-from-files");

    CreateCoreWebView2EnvironmentWithOptions(                                 //
        nullptr,                                                              //
        nullptr,                                                              //
        options.Get(),                                                        //
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>( //
            [hWnd](HRESULT result,                                            //
                   ICoreWebView2Environment *env) -> HRESULT {                //
                return OnEnvironmentCreated(hWnd, result, env);               //
            })                                                                //
            .Get()                                                            //
    );                                                                        //
}