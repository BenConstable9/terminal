#include "pch.h"

#include "CTerminalHandoff.h"

// The callback function when a connection is received
static NewHandoff _pfnHandoff = nullptr;
// The registration ID of the class object for clean up later
static DWORD g_cTerminalHandoffRegistration = 0;

// Routine Description:
// - Starts listening for TerminalHandoff requests by registering
//   our class and interface with COM.
// Arguments:
// - pfnHandoff - Function to callback when a handoff is received
// Return Value:
// - S_OK, E_NOT_VALID_STATE (start called when already started) or relevant COM registration error.
HRESULT CTerminalHandoff::s_StartListening(NewHandoff pfnHandoff) noexcept
try
{
    RETURN_HR_IF(E_NOT_VALID_STATE, _pfnHandoff != nullptr);

    // We could probably hold this in a static...
    auto classFactory = Make<SimpleClassFactory<CTerminalHandoff>>();

    RETURN_IF_NULL_ALLOC(classFactory);

    ComPtr<IUnknown> unk;
    RETURN_IF_FAILED(classFactory.As(&unk));

    RETURN_IF_FAILED(CoRegisterClassObject(__uuidof(CTerminalHandoff), unk.Get(), CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &g_cTerminalHandoffRegistration));

    _pfnHandoff = pfnHandoff;

    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Stops listening for TerminalHandoff requests by revoking the registration
//   our class and interface with COM
// Arguments:
// - <none>
// Return Value:
// - S_OK, E_NOT_VALID_STATE (stop called when not started), or relevant COM class revoke error
HRESULT CTerminalHandoff::s_StopListening() noexcept
{
    RETURN_HR_IF(E_NOT_VALID_STATE, _pfnHandoff == nullptr);

    _pfnHandoff = nullptr;

    if (g_cTerminalHandoffRegistration)
    {
        RETURN_IF_FAILED(CoRevokeClassObject(g_cTerminalHandoffRegistration));
        g_cTerminalHandoffRegistration = 0;
    }

    return S_OK;
}

// Routine Description:
// - Helper to duplicate a handle to ourselves so we can keep holding onto it
//   after the caller frees the original one.
// Arguments:
// - in - Handle to duplicate
// - out - Where to place the duplicated value
// Return Value:
// - S_OK or Win32 error from `::DuplicateHandle`
HRESULT _duplicateHandle(const HANDLE in, HANDLE& out) noexcept
{
    RETURN_IF_WIN32_BOOL_FALSE(::DuplicateHandle(GetCurrentProcess(), in, GetCurrentProcess(), &out, 0, FALSE, DUPLICATE_SAME_ACCESS));
    return S_OK;
}

HRESULT CTerminalHandoff::EstablishHandoff(HANDLE in, HANDLE out, HANDLE signal) noexcept
{
    // Duplicate the handles from what we received.
    // The contract with COM specifies that any HANDLEs we receive from the caller belong
    // to the caller and will be freed when we leave the scope of this method.
    // Making our own duplicate copy ensures they hang around in our lifetime.
    RETURN_IF_FAILED(_duplicateHandle(in, in));
    RETURN_IF_FAILED(_duplicateHandle(out, out));
    RETURN_IF_FAILED(_duplicateHandle(signal, signal));

    // Call registered handler from when we started listening.
    return _pfnHandoff(in, out, signal);
}