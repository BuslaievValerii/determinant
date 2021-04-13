#include <windows.h>
#include <cmath>
#include <string>
#include <ctime>
using namespace std;

#define WM_THREAD_FINISH WM_USER + 10

struct matrix
{
    int matrix_size;
    int* matrix;
};


struct thread_args
{
    matrix* matr;
    long result;
    int column;
};


struct calculate_args
{
    int mode;
    matrix* matr;
    bool prioritize;
};


CRITICAL_SECTION sec;


int* get_minor(matrix* mat, int column)
{
    int size = mat->matrix_size - 1;
    int* minor = new int[size * size];
    for (int i = 0, pos = 0; i < mat->matrix_size * size; i++)
    {
        int j = i % mat->matrix_size;
        if (j != column)
        {
            minor[pos] = mat->matrix[i];
            pos++;
        }
    }
    return minor;
}


long calc_det(matrix* mat)
{
    long result = 0;
    if (mat->matrix_size > 3)
    {
        for (int i = 0; i < mat->matrix_size; i++)
        {
            int* minor = get_minor(mat, i);
            int el = mat->matrix[mat->matrix_size * (mat->matrix_size - 1) + i];
            matrix m = { mat->matrix_size - 1, minor };
            long det = calc_det(&m);
            result += pow(-1, i + mat->matrix_size + 1) * el * det;
            delete[] minor;
        }
    }
    else
    {
        int* m = mat->matrix;
        result = m[0] * m[4] * m[8] - m[0] * m[5] * m[7] - m[1] * m[3] * m[8] - m[2] * m[4] * m[6] + m[1] * m[5] * m[6] + m[2] * m[3] * m[7];
    }
    return result;
}


DWORD WINAPI thread_func(LPVOID _args)
{
    thread_args* args = (thread_args*)_args;
    int* minor = get_minor(args->matr, args->column);
    int el = args->matr->matrix[args->matr->matrix_size * (args->matr->matrix_size - 1) + args->column];
    matrix m = {args->matr->matrix_size - 1, minor };
    long det = calc_det(&m);
    EnterCriticalSection(&sec);
        args->result = pow(-1, args->column + args->matr->matrix_size + 1) * el * det;
    LeaveCriticalSection(&sec);
    delete[] minor;
    return 1;
}

HWND hwnd;

HWND hbtn_start;
HWND hbtn_stop;
HWND hbtn_pause;
HWND label_result;
HWND matrix_place;
HWND input;

bool running = false;

HANDLE* threads;
int threads_count;

long result = 0;
LARGE_INTEGER start_time, end_time, frequency;
ULONGLONG delta = 0;

matrix mat;
calculate_args calculation;

matrix& create_matrix(int size)
{
    srand(time(NULL));
    int* array = new int[size*size];

    for (int i = 0; i < size * size; i++)
    {
        array[i] = rand() % 20;
    }

    matrix mat = { size, array };
    return mat;
}


DWORD WINAPI calculate(LPVOID _args)
{
    calculate_args* params = (calculate_args*)_args;
    switch (params->mode)
    {
    case IDYES:
    {
        threads_count = params->matr->matrix_size;
        threads = new HANDLE[threads_count];
        thread_args* args = new thread_args[threads_count];
        int priority = params->prioritize
            ? THREAD_PRIORITY_ABOVE_NORMAL
            : THREAD_PRIORITY_NORMAL;
        QueryPerformanceCounter(&start_time);
        for (int i = 0; i < params->matr->matrix_size; i++)
        {
            args[i].matr = params->matr;
            args[i].column = i;
            threads[i] = CreateThread(NULL, 0, thread_func, &args[i], 0, NULL);
            SetThreadPriority(threads[i], priority);
        }

        WaitForMultipleObjects(params->matr->matrix_size, threads, true, INFINITE);

        for (int i = 0; i < params->matr->matrix_size; i++)
        {
            result += args[i].result;
        }
        QueryPerformanceCounter(&end_time);

        delete[] args;
        delete[] threads;
        threads_count = 0;
    }
    break;

    case IDNO:
        QueryPerformanceCounter(&start_time);
        result = calc_det(params->matr);
        QueryPerformanceCounter(&end_time);
        break;
    }
    delete[] params->matr->matrix;
    PostMessage(hwnd, WM_THREAD_FINISH, 0, 0);
    return 0;
}


void start_threads()
{
    TCHAR buff[512];
    GetWindowText(input, buff, 512);
    int size = max(_wtoi(buff), 4);
    mat = create_matrix(size);

    wstring matrix_str;
    for (int i = 0; i < mat.matrix_size*mat.matrix_size; i++)
    {
        if (i % mat.matrix_size == 0 && i != 0)
        {
            matrix_str += L"\n";
        }
        if (mat.matrix[i] < 10)
        {
            matrix_str += L" ";
        }
        matrix_str += to_wstring(mat.matrix[i]) + L" ";
    }
    SetWindowText(matrix_place, matrix_str.c_str());
    EnableWindow(hbtn_start, false);

    int mode = MessageBox(NULL, L"Wanna use threads?", L"Choose the mode", MB_ICONQUESTION | MB_YESNO);

    bool prioritize = mode == IDYES
        ? (MessageBox(NULL, L"Set highest priority?", L"Choose the mode", MB_ICONQUESTION | MB_YESNO) == IDYES)
        : false;

    calculation = {mode, &mat, prioritize};

    CreateThread(NULL, 0, calculate, &calculation , 0, NULL);
    running = true;
    EnableWindow(hbtn_pause, true);
}


void pause_threads()
{
    if (running)
    {
        for (int i = 0; i < threads_count ; i++)
        {
            SuspendThread(threads[i]);
        }
        SetWindowText(hbtn_pause, L"Resume");
    }
    else
    {
        for (int i = 0; i < threads_count; i++)
        {
            ResumeThread(threads[i]);
        }
        SetWindowText(hbtn_pause, L"Pause");
    }
    running = !running;
}


void stop_threads()
{
    for (int i = 0; i < threads_count; i++)
    {
        TerminateThread(threads[i], 0);
    }
}


LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_COMMAND:
        if (lparam != 0 && HIWORD(wparam) == BN_CLICKED)
        {
            HWND hbtn = (HWND)lparam;
            if (hbtn == hbtn_start)
            {
                start_threads();
            }
            else if (hbtn == hbtn_stop)
            {
                stop_threads();
                PostQuitMessage(0);
            }
            else if (hbtn == hbtn_pause)
            {
                pause_threads();
            }
        }
        break;
    case WM_THREAD_FINISH:
    {
        delta = end_time.QuadPart - start_time.QuadPart;
        delta *= 1000000000;
        QueryPerformanceFrequency(&frequency);
        delta /= frequency.QuadPart;

        wstring message_res = wstring(L"Result: ") + to_wstring(result) + wstring(L"\nTime: ") + to_wstring(delta) + wstring(L" ns");
        SetWindowText(label_result, message_res.c_str());
        EnableWindow(hbtn_start, true);
        EnableWindow(hbtn_pause, false);
        running = false;
    }
        break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}


int WINAPI WinMain(HINSTANCE hInst,
    HINSTANCE hPreviousInst,
    LPSTR lpCommandLine,
    int nCommandShow)
{
    InitializeCriticalSection(&sec);
    const wchar_t classname[] = L"My window";

    WNDCLASS wc = {};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hInst;
    wc.lpszClassName = classname;
    RegisterClass(&wc);

    hwnd = CreateWindowEx(0, classname, L"Lab 1 Buslaiev", WS_OVERLAPPEDWINDOW &~ WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, 435, 600, NULL, NULL, hInst, NULL);
    if (hwnd == NULL)
    {
        MessageBox(NULL, L"Couldn't create the window", L"Error", MB_OK);
        return NULL;
    }

    hbtn_start = CreateWindow(L"Button", L"Start", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 10, 10, 100, 50, hwnd, NULL, hInst, NULL);
    hbtn_pause = CreateWindow(L"Button", L"Pause", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 120, 10, 100, 50, hwnd, NULL, hInst, NULL);
    hbtn_stop = CreateWindow(L"Button", L"Stop", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 230, 10, 100, 50, hwnd, NULL, hInst, NULL);

    input = CreateWindow(L"Edit", L"", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER, 340, 10, 50, 50, hwnd, NULL, hInst, NULL);

    matrix_place = CreateWindow(L"Static", L"", WS_VISIBLE | WS_CHILD, 10, 70, 400, 400, hwnd, NULL, hInst, NULL);
    label_result = CreateWindow(L"Static", L"", WS_VISIBLE | WS_CHILD, 10, 480, 400, 50, hwnd, NULL, hInst, NULL);

    SetWindowText(label_result, L"Result: ");
    EnableWindow(hbtn_pause, false);

    ShowWindow(hwnd, nCommandShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteCriticalSection(&sec);

    return NULL;
}
