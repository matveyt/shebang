/*
 * Proj: shebang
 * Auth: matveyt
 * Desc: Allows direct execution of MSYS/Cygwin shebang scripts
 * Note: Rename or symlink to match the desired script and put both on PATH
 */


/** Build instructions:

    -DUNICODE = builds 'unicode' version instead of 'ansi'
    -DNOSTDLIB (also requires -O) = no libc dependencies, useful for GCC -nostdlib build

**/


#if __STDC_VERSION__ < 199901L && !defined(__POCC__)
#error C99 compiler required.
#endif


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>


#ifndef __GNUC__
#pragma comment(lib, "shlwapi.lib")
#endif // __GNUC__


// GCC/MinGW has no libstrsafe.a separate from libmingwex.a,
// so we have to inline it in order to get rid of libc
#ifdef NOSTDLIB
#ifdef __GNUC__
#ifdef __NO_INLINE__
#error Option -O is required.
#endif // __NO_INLINE__
#define _FORCE_INLINE inline __attribute__ ((always_inline))
#ifdef UNICODE
_FORCE_INLINE HRESULT WINAPI StringCchLengthW(LPCWSTR,size_t,size_t*);
_FORCE_INLINE HRESULT WINAPI StringLengthWorkerW(LPCWSTR,size_t,size_t*);
_FORCE_INLINE HRESULT WINAPI StringCchCopyW(LPWSTR,size_t,LPCWSTR);
_FORCE_INLINE HRESULT WINAPI StringCopyWorkerW(LPWSTR,size_t,LPCWSTR);
_FORCE_INLINE HRESULT WINAPI StringCchCopyNW(LPWSTR,size_t,LPCWSTR,size_t);
_FORCE_INLINE HRESULT WINAPI StringCopyNWorkerW(LPWSTR,size_t,LPCWSTR,size_t);
_FORCE_INLINE HRESULT WINAPI StringCchCatW(LPWSTR,size_t,LPCWSTR);
_FORCE_INLINE HRESULT WINAPI StringCatWorkerW(LPWSTR,size_t,LPCWSTR);
#else
_FORCE_INLINE HRESULT WINAPI StringCchLengthA(LPCSTR,size_t,size_t*);
_FORCE_INLINE HRESULT WINAPI StringLengthWorkerA(LPCSTR,size_t,size_t*);
_FORCE_INLINE HRESULT WINAPI StringCchCopyA(LPSTR,size_t,LPCSTR);
_FORCE_INLINE HRESULT WINAPI StringCopyWorkerA(LPSTR,size_t,LPCSTR);
_FORCE_INLINE HRESULT WINAPI StringCchCopyNA(LPSTR,size_t,LPCSTR,size_t);
_FORCE_INLINE HRESULT WINAPI StringCopyNWorkerA(LPSTR,size_t,LPCSTR,size_t);
_FORCE_INLINE HRESULT WINAPI StringCchCatA(LPSTR,size_t,LPCSTR);
_FORCE_INLINE HRESULT WINAPI StringCatWorkerA(LPSTR,size_t,LPCSTR);
#endif // UNICODE
#endif // __GNUC__
#endif // NOSTDLIB
#include <strsafe.h>


// our original name
#define PROGRAM_NAME    "shebang"
// macro to facilitate function calling
#define COUNT(a)        (sizeof(a) / sizeof(*a))
#define ARRAY(a)        (a), COUNT(a)
// latin letter test
#define IS_LATIN(c)     (('A' <= (c) && (c) <= 'Z') || ('a' <= (c) && (c) <= 'z'))


// known POSIX layers
typedef struct {
    enum {
        POSIX_UNKNOWN = -1,
        POSIX_MINGW32,  // MINGW32
        POSIX_MINGW64,  // MINGW64
        POSIX_UCRT64,   // UCRT64
        POSIX_CLANG32,  // CLANG32
        POSIX_CLANG64,  // CLANG64
        POSIX_MSYS,     // MSYS
        POSIX_CYGWIN,   // Cygwin
        POSIX_COUNT
    } sys;
    TCHAR root[MAX_PATH];
} POSIX;


// internal implementation of memcmp()
int compare_bytes(const void* s1, const void* s2, size_t n)
{
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;

    do {
        int b1 = *p1++;
        int b2 = *p2++;
        if (b1 != b2)
            return (b1 - b2);
    } while (--n);

    return 0;
}


// concats TCHAR string with UTF-8 string
BOOL concat_with_utf8(PTSTR pszDest, size_t cchDest, const char* src)
{
    size_t cnt;
    if (FAILED(StringCchLength(pszDest, cchDest, &cnt)))
        return FALSE;

#ifdef UNICODE
    return (MultiByteToWideChar(CP_UTF8, 0, src, -1, pszDest + cnt, cchDest - cnt) > 0);
#else
    wchar_t tmp[cchDest - cnt]; // VLA
    return (MultiByteToWideChar(CP_UTF8, 0, src, -1, tmp, cchDest - cnt) > 0
        && WideCharToMultiByte(CP_ACP, 0, tmp, -1, pszDest + cnt, cchDest - cnt,
            NULL, NULL) > 0);
#endif // UNICODE
}


// replaces all occurences of a char in a string
void replace_char(PTSTR psz, TCHAR cTo, TCHAR cFrom)
{
    for ( ; *psz; ++psz)
        if (*psz == cFrom)
            *psz = cTo;
}


// finds active MSYS/Cygwin installation by scanning PATH
void find_posix(POSIX* ppx)
{
    // nothing found yet
    ppx->sys = POSIX_UNKNOWN;

    // get PATH
    TCHAR achPATH[4096]; // max
    size_t cchPATH = (size_t)GetEnvironmentVariable(TEXT("PATH"), ARRAY(achPATH));
    if (!cchPATH || cchPATH > COUNT(achPATH))
        return; // unexpected error

    ++cchPATH; // count null-terminating character
    replace_char(achPATH, TEXT('\0'), TEXT(';')); // split PATH

    // find first path matching one of the patterns
    TCHAR* cp;
    size_t cch;
    for (cp = achPATH; cchPATH; cp += cch + 1, cchPATH -= cch + 1) {
        if (FAILED(StringCchLength(cp, cchPATH, &cch)))
            break; // unexpected error
        if (PathMatchSpec(cp, TEXT("*\\msys*\\mingw32\\bin"))) {
            // found MINGW32
            ppx->sys = POSIX_MINGW32;
            cch -= COUNT("\\mingw32\\bin") - 1;
            break;
        }
        if (PathMatchSpec(cp, TEXT("*\\msys*\\mingw64\\bin"))) {
            // found MINGW64
            ppx->sys = POSIX_MINGW64;
            cch -= COUNT("\\mingw64\\bin") - 1;
            break;
        }
        if (PathMatchSpec(cp, TEXT("*\\msys*\\ucrt64\\bin"))) {
            // found UCRT64
            ppx->sys = POSIX_UCRT64;
            cch -= COUNT("\\ucrt64\\bin") - 1;
            break;
        }
        if (PathMatchSpec(cp, TEXT("*\\msys*\\clang32\\bin"))) {
            // found CLANG32
            ppx->sys = POSIX_CLANG32;
            cch -= COUNT("\\clang32\\bin") - 1;
            break;
        }
        if (PathMatchSpec(cp, TEXT("*\\msys*\\clang64\\bin"))) {
            // found CLANG64
            ppx->sys = POSIX_CLANG64;
            cch -= COUNT("\\clang64\\bin") - 1;
            break;
        }
        if (PathMatchSpec(cp, TEXT("*\\msys*\\usr\\bin"))) {
            // found MSYS
            ppx->sys = POSIX_MSYS;
            cch -= COUNT("\\usr\\bin") - 1;
            break;
        }
        if (PathMatchSpec(cp, TEXT("*\\cygwin*\\bin"))) {
            // found Cygwin
            ppx->sys = POSIX_CYGWIN;
            cch -= COUNT("\\bin") - 1;
            break;
        }
    }

    if (ppx->sys != POSIX_UNKNOWN) // wipe last dirs to get the root
        StringCchCopyN(ARRAY(ppx->root), cp, cch);
}


// sets some of MSYS/Cygwin environment variables
void setup_posix_env(POSIX* ppx)
{
    // MSYS names and POSIX prefixes
    static PCTSTR const pszMSYS[POSIX_COUNT][2] = {
        { TEXT("MINGW32"), TEXT("/mingw32") },
        { TEXT("MINGW64"), TEXT("/mingw64") },
        { TEXT("UCRT64"), TEXT("/ucrt64") },
        { TEXT("CLANG32"), TEXT("/clang32") },
        { TEXT("CLANG64"), TEXT("/clang64") },
        { TEXT("MSYS"), TEXT("/usr") },
        { NULL, NULL }
    };

    // set MSYSTEM, MSYSTEM_PREFIX and MINGW_PREFIX
    SetEnvironmentVariable(TEXT("MSYSTEM"), pszMSYS[ppx->sys][0]);
    SetEnvironmentVariable(TEXT("MSYSTEM_PREFIX"), pszMSYS[ppx->sys][1]);
    SetEnvironmentVariable(TEXT("MINGW_PREFIX"),
        ppx->sys == POSIX_MSYS ? NULL : pszMSYS[ppx->sys][1]);

    // set USER and HOSTNAME
    TCHAR tmp[256]; // max
    if (GetEnvironmentVariable(TEXT("USERNAME"), ARRAY(tmp)))
        SetEnvironmentVariable(TEXT("USER"), tmp);
    if (GetComputerNameEx(ComputerNameDnsHostname, tmp, &(DWORD) { COUNT(tmp) }))
        SetEnvironmentVariable(TEXT("HOSTNAME"), tmp);
}


// converts POSIX path to native path
BOOL convert_path(PTSTR pszTo, size_t cchTo, POSIX* ppx, const char* from)
{
    TCHAR tmp[MAX_PATH];

    if (*from == '\\' || (IS_LATIN(from[0]) && from[1] == ':')) { // Win path
        tmp[0] = TEXT('\0');
    } else if (*from == '/') { // POSIX path
        ++from;

        // skip over cygdrive/
        if (!compare_bytes(from, ARRAY("cygdrive/") - 1))
            from += COUNT("cygdrive/") - 1;

        // substitute: /c --> c:
        if (IS_LATIN(from[0]) && from[1] == '/') {
            tmp[0] = (TCHAR)from[0];
            tmp[1] = TEXT(':');
            tmp[2] = TEXT('/');
            tmp[3] = TEXT('\0');
            from += COUNT("c/") - 1;
        } else {
            // start from POSIX root
            StringCchCopy(ARRAY(tmp), ppx->root);
            StringCchCat(ARRAY(tmp), TEXT("/"));

            if (ppx->sys == POSIX_CYGWIN) {
                // /usr/bin --> /bin
                if (!compare_bytes(from, ARRAY("usr/bin/") - 1)) {
                    StringCchCat(ARRAY(tmp), TEXT("bin/"));
                    from += COUNT("usr/bin/") - 1;
                }
            }  else {
                // /bin --> /usr/bin
                if (!compare_bytes(from, ARRAY("bin/") - 1)) {
                    StringCchCat(ARRAY(tmp), TEXT("usr/bin/"));
                    from += COUNT("bin/") - 1;
                }
            }
        }
    } else { // relative path
        GetCurrentDirectory(COUNT(tmp), tmp);
        StringCchCat(ARRAY(tmp), TEXT("/"));
    }

    // add the rest
    if (!concat_with_utf8(ARRAY(tmp), from))
        return FALSE;

    // apply native separators and .exe extension
    replace_char(tmp, TEXT('\\'), TEXT('/')); // to Win separators
    PathAddExtension(tmp, NULL); // note: PathCchAddExtension is for Win 8+ only

    // check if file exists
    if (!PathFileExists(tmp))
        return FALSE;

    // executable name containing spaces should be enquoted for security reasons
    PathQuoteSpaces(tmp);

    // copy out result
    return SUCCEEDED(StringCchCopy(pszTo, cchTo, tmp));
}


// parses a shebang line
BOOL parse_line(char* line, size_t cnt, const char** ppc1, const char** ppc2)
{
    char* cp;

    // #!
    if (cnt < 2 || *line++ != '#' || *line++ != '!')
        return FALSE;
    cnt -= 2;

    // convert tabs to spaces, break at a newline
    for (cp = line; cnt; ++cp, --cnt) {
        if (*cp == '\0') // unexpected end of string
            return FALSE;
        if (*cp == '\n' || *cp == '\r') {
            *cp = '\0';
            break;
        }
        if (*cp == '\t')
            *cp = ' ';
    }
    if (!cnt) // no newline
        return FALSE;

    // skip leading spaces
    for (cp = line; *cp == ' '; ++cp) ;
    if (*cp == '\0') // shell not found
        return FALSE;

    // store pointer to shell name
    *ppc1 = cp;

    // skip until next space
    while (*cp && *cp != ' ') ++cp;

    if (*cp == ' ') {
        // have args
        *cp++ = '\0';
        *ppc2 = cp;
    } else {
        // no args
        *ppc2 = NULL;
    }

    return TRUE;
}


// checks if file is a shell script
BOOL can_shebang(PCTSTR pszScriptName, PTSTR pszShellName, size_t cchShellName,
    POSIX* ppx, PDWORD pdwErrorCode)
{
    // open script file
    HANDLE hScriptFile = CreateFile(pszScriptName, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hScriptFile == INVALID_HANDLE_VALUE) {
        *pdwErrorCode = GetLastError();
        return FALSE;
    }

    BOOL shebang = FALSE;
    char buf[cchShellName]; // VLA
    DWORD cb;
    const char *pc1, *pc2;
    if (!ReadFile(hScriptFile, buf, cchShellName, &cb, NULL)) {
        // cannot read file
        *pdwErrorCode = GetLastError();
    } else if (!parse_line(buf, (size_t)cb, &pc1, &pc2)) {
        // not a script
        *pdwErrorCode = ERROR_BAD_FORMAT;
    } else if (!convert_path(pszShellName, cchShellName, ppx, pc1)) {
        // invalid shell
        *pdwErrorCode = ERROR_PATH_NOT_FOUND;
    } else {
        // process shebang args if any
        shebang = pc2 ? (
            SUCCEEDED(StringCchCat(pszShellName, cchShellName, TEXT(" "))) &&
            concat_with_utf8(pszShellName, cchShellName, pc2)
        ) : TRUE;
        if (!shebang) // invalid shebang args
            *pdwErrorCode = ERROR_BAD_ARGUMENTS;
    }

    CloseHandle(hScriptFile);
    return shebang;
}


// prints error message and quits the application
__declspec(noreturn)
void print_error_and_exit(DWORD dwErrorCode)
{
    LPTSTR pszErrorText;
    DWORD cchErrorText = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
        dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&pszErrorText,
        0, NULL);
    if (cchErrorText > 0) {
        HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
        DWORD cb;
        WriteConsole(hStdErr, ARRAY(TEXT(PROGRAM_NAME " error: ")) - 1, &cb, NULL);
        WriteConsole(hStdErr, pszErrorText, cchErrorText, &cb, NULL);
        // no need to free memory before exit
        //HeapFree(GetProcessHeap(), 0, pszErrorText);
    }
    ExitProcess(dwErrorCode);
}


// application standard entry point
#ifdef UNICODE
int wmain(void)
#else
int main(void)
#endif // UNICODE
{
    DWORD dwErrorCode;

#ifndef UNICODE
    // get rid of OEM codepage
    SetConsoleOutputCP(GetACP());
#endif // UNICODE

    // find our basename
    TCHAR szName[MAX_PATH];
    GetModuleFileName(NULL, ARRAY(szName));
    PathStripPath(szName);
    PathRemoveExtension(szName); // note: PathCchRemoveExtension is for Win 8+ only

    // check if we're renamed
    if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, szName, -1,
        TEXT(PROGRAM_NAME), -1) == CSTR_EQUAL) {
        WriteConsole(GetStdHandle(STD_ERROR_HANDLE), ARRAY(TEXT(
"Hello Windows(R) world! It\'s me, humble \'" PROGRAM_NAME "\' utility.\n\n"
"I can help you to execute MSYS/Cygwin shell scripts from your native \'cmd\',\n"
"but only if you already have it on your PATH.\n\n"
"All you have to do is to rename or symlink me, so that I match the script you want.\n"
"And, of course, please, make sure that we\'re both on the PATH too.\n\n"
"Let\'s do it!\n\n"
            )) - 1, &(DWORD) { 0 }, NULL);
        print_error_and_exit(ERROR_CANT_RESOLVE_FILENAME);
    }

    // find POSIX root
    POSIX px;
    find_posix(&px);
    if (px.sys == POSIX_UNKNOWN)
        print_error_and_exit(ERROR_INVALID_ENVIRONMENT);

    // find matching shell script on PATH
    if (!PathFindOnPath(szName, NULL))
        print_error_and_exit(ERROR_FILE_NOT_FOUND);

    // can she bang?
    TCHAR szShellCmd[MAX_PATH];
    if (!can_shebang(szName, ARRAY(szShellCmd), &px, &dwErrorCode))
        print_error_and_exit(dwErrorCode);

    // prepare script name for passing onto the shell
    PathQuoteSpaces(szName);
    replace_char(szName, TEXT('/'), TEXT('\\')); // to POSIX separators

    // make command line
    TCHAR szCmdLine[32768]; // max
    StringCchCopy(ARRAY(szCmdLine), szShellCmd);    // shell + shebang args
    StringCchCat(ARRAY(szCmdLine), TEXT(" "));      // space
    StringCchCat(ARRAY(szCmdLine), szName);         // script
    PTSTR pszRawArgs = PathGetArgs(GetCommandLine());
    if (pszRawArgs && *pszRawArgs) {
        StringCchCat(ARRAY(szCmdLine), TEXT(" "));  // space
        StringCchCat(ARRAY(szCmdLine), pszRawArgs); // our args
    }

    // setup POSIX environment
    setup_posix_env(&px);

    // launch shell
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, 0, NULL, NULL,
        &(STARTUPINFO) { .cb = sizeof(STARTUPINFO) }, &pi))
        print_error_and_exit(GetLastError());

    // wait for the child and exit
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &dwErrorCode);
    // no need to close handles before exit
    //CloseHandle(pi.hProcess);
    //CloseHandle(pi.hThread);
    return (int)dwErrorCode;
}
