#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/RngLib.h>

#define SNAKE_DIRECTION_UP (UINT8)0
#define SNAKE_DIRECTION_LEFT (UINT8)1
#define SNAKE_DIRECTION_DOWN (UINT8)3
#define SNAKE_DIRECTION_RIGHT (UINT8)4

#define SNAKE_BODY_SYMBOL (CHAR16*)L"@"
#define SNAKE_FRUIT_SYMBOL (CHAR16*)L"O"
#define SNAKE_BORDER_SYMBOL (CHAR16*)L"#"

typedef struct snake_body_node_s {
    UINT16 x, y;
} snake_body_node_s;

typedef struct snake_body_node_s snake_fruit_s;

typedef struct snake_game_s {
    UINT64 tick_rate_ms;
    UINT16 score;

    UINT8 direction;

    UINT16 border_width, border_height;

    snake_body_node_s *body;
    UINT32 body_len;
    
    snake_fruit_s fruit;

    BOOLEAN running;

} snake_game_s;

void snake_fruit_relocate(snake_game_s *const pSnakeGame)
{
    if(!pSnakeGame || !gBS || !gST)
        return;

    snake_fruit_s sAvailSpaces[(pSnakeGame->border_width - 2) * (pSnakeGame->border_height - 2)];
    UINT32 iAvailSpacesLen = 0;

    for(UINT16 y = 1; y <= pSnakeGame->border_height - 2; ++y)
    {
        snake_body_node_s pSnakeNodesY[pSnakeGame->body_len];
        UINT32 iSnakeNodesYCount = 0;
            
        for(UINT32 i = 0; i < pSnakeGame->body_len; ++i)
        {
            if(pSnakeGame->body[i].y == y)
            {
                pSnakeNodesY[iSnakeNodesYCount] = pSnakeGame->body[i];
                ++iSnakeNodesYCount;
            }
        }

        for(UINT16 x = 1; x <= pSnakeGame->border_width - 2; ++x)
        {
            if(pSnakeGame->fruit.x == x && pSnakeGame->fruit.y == y)
                continue;
            
            BOOLEAN bIsSnake = FALSE;
            for(UINT32 i = 0; i < iSnakeNodesYCount; ++i)
            {
                if(pSnakeNodesY[i].x == x)
                {    
                    bIsSnake = TRUE;
                    break;
                }
            }
            if(bIsSnake)
                continue;

            sAvailSpaces[iAvailSpacesLen].x = x;
            sAvailSpaces[iAvailSpacesLen].y = y;
            ++iAvailSpacesLen;
        }
    }

    if(!iAvailSpacesLen)
        return;

    UINT32 iRNG;
    GetRandomNumber32(&iRNG);

    const UINT32 iIndex = iRNG % iAvailSpacesLen;
    pSnakeGame->fruit.x = sAvailSpaces[iIndex].x;
    pSnakeGame->fruit.y = sAvailSpaces[iIndex].y;
}

BOOLEAN snake_init(snake_game_s *const pSnakeGame, const UINT64 tick_rate_ms)
{
    if(!pSnakeGame || !gST || !gBS)
        return FALSE;

    UINT32 iModeIx = 0;
    UINT16 iBestRows = 0;
    UINT16 iBestCols = 0;

    for(UINT32 cur_mode = iModeIx; cur_mode < gST->ConOut->Mode->MaxMode; ++cur_mode)
    {
        UINTN iRows, iColumns;
        if(gST->ConOut->QueryMode(gST->ConOut, (UINTN)cur_mode, &iColumns, &iRows) != EFI_SUCCESS)
            continue;
        if((iRows * iColumns) > (iBestRows * iBestCols))
        {
            iModeIx = cur_mode;
            iBestRows = (UINT16)iRows;
            iBestCols = (UINT16)iColumns;
        }
    }
    if((iBestCols * iBestRows) == 0)
        return FALSE;

    pSnakeGame->border_width = 24;
    pSnakeGame->border_height = 24;

    if(gBS->AllocatePool(EfiLoaderData, (UINTN)((pSnakeGame->border_width * pSnakeGame->border_height) * sizeof(snake_body_node_s)), (void**)&pSnakeGame->body))
        return FALSE;

    //gST->ConOut->SetMode(gST->ConOut, (UINTN)iModeIx);
    gST->ConOut->EnableCursor(gST->ConOut, FALSE);
    //gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

    pSnakeGame->running = FALSE;

    pSnakeGame->direction = SNAKE_DIRECTION_UP;
    pSnakeGame->tick_rate_ms = tick_rate_ms;
    
    pSnakeGame->body_len = 1;
    pSnakeGame->score = 0;

    pSnakeGame->body[0].x = (pSnakeGame->border_width - 2) / 2;
    pSnakeGame->body[0].y = (pSnakeGame->border_height - 2) / 2;

    snake_fruit_relocate(pSnakeGame);

    return TRUE;
}

void snake_clean(snake_game_s *const pSnakeGame)
{
    if(!pSnakeGame || !gBS || !gST)
        return;

    pSnakeGame->running = FALSE;

    gST->ConOut->EnableCursor(gST->ConOut, TRUE);
    //gST->ConOut->SetMode(gST->ConOut, 0);
    //gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

    gBS->FreePool(pSnakeGame->body);
}

BOOLEAN snake_move(snake_game_s *const pSnakeGame)
{
    if(!pSnakeGame)
        return FALSE;

    UINT16 iPrevX = pSnakeGame->body[0].x;
    UINT16 iPrevY = pSnakeGame->body[0].y;

    switch(pSnakeGame->direction)
    {
        case SNAKE_DIRECTION_UP:
        {
            --pSnakeGame->body[0].y;
            break; 
        }
        case SNAKE_DIRECTION_LEFT:
        {
            --pSnakeGame->body[0].x;
            break;
        }
        case SNAKE_DIRECTION_DOWN:
        {
            ++pSnakeGame->body[0].y;
            break;
        }
        case SNAKE_DIRECTION_RIGHT:
        {
            ++pSnakeGame->body[0].x;
            break;
        }
        default:
            return FALSE;
    }

    if(pSnakeGame->body[0].x <= 0 || pSnakeGame->body[0].x >= (pSnakeGame->border_width - 1) || pSnakeGame->body[0].y <= 0 || pSnakeGame->body[0].y >= (pSnakeGame->border_height - 1))
        return FALSE;

    for(UINT32 i = 1; i < pSnakeGame->body_len; ++i)
    {
        const UINT16 iCurrentX = pSnakeGame->body[i].x;
        const UINT16 iCurrentY = pSnakeGame->body[i].y;
        
        pSnakeGame->body[i].x = iPrevX;
        pSnakeGame->body[i].y = iPrevY;

        if(pSnakeGame->body[0].x == iPrevX && pSnakeGame->body[0].y == iPrevY)
            return FALSE;

        iPrevX = iCurrentX;
        iPrevY = iCurrentY;
    }

    return TRUE;
}

BOOLEAN snake_collect_fruit(snake_game_s *const pSnakeGame)
{
    if(!pSnakeGame || pSnakeGame->body_len >= (pSnakeGame->border_width * pSnakeGame->border_height))
        return FALSE;

    ++pSnakeGame->score;
    ++pSnakeGame->body_len;  

    snake_fruit_relocate(pSnakeGame);
    
    return TRUE;
}

void snake_restart(snake_game_s *const pSnakeGame)
{
    if(!pSnakeGame)
        return;
    
    pSnakeGame->direction = SNAKE_DIRECTION_UP;
    
    pSnakeGame->body_len = 1;
    pSnakeGame->score = 0;

    pSnakeGame->body[0].x = (pSnakeGame->border_width - 2) / 2;
    pSnakeGame->body[0].y = (pSnakeGame->border_height - 2) / 2;

    snake_fruit_relocate(pSnakeGame);
}

void snake_listen_kb(snake_game_s *const pSnakeGame)
{
    if(!pSnakeGame || !gST || !gBS)
        return;

    EFI_INPUT_KEY sKey = { 0 };
    gST->ConIn->ReadKeyStroke(gST->ConIn, &sKey);

    switch(sKey.ScanCode)
    {
        case SCAN_ESC:
        {
            pSnakeGame->running = FALSE;
            break;
        }
        case SCAN_F1:
        {
            snake_restart(pSnakeGame);
            break;
        }
        
        case SCAN_UP:
        {
            pSnakeGame->direction = pSnakeGame->direction != SNAKE_DIRECTION_DOWN ? SNAKE_DIRECTION_UP : pSnakeGame->direction;
            break;
        }
        case SCAN_LEFT:
        {
            pSnakeGame->direction = pSnakeGame->direction != SNAKE_DIRECTION_RIGHT ? SNAKE_DIRECTION_LEFT : pSnakeGame->direction;
            break;
        }
        case SCAN_DOWN:
        {
            pSnakeGame->direction = pSnakeGame->direction != SNAKE_DIRECTION_UP ? SNAKE_DIRECTION_DOWN : pSnakeGame->direction;
            break;
        }
        case SCAN_RIGHT:
        {
            pSnakeGame->direction = pSnakeGame->direction != SNAKE_DIRECTION_LEFT ? SNAKE_DIRECTION_RIGHT : pSnakeGame->direction;
            break;
        }
        default:
            break;
    }
}

void snake_draw(snake_game_s *const pSnakeGame)
{
    if(!pSnakeGame || !gST || !gBS)
        return;

    gST->ConOut->ClearScreen(gST->ConOut);
    //gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

    for(UINT16 y = 0; y < pSnakeGame->border_height; ++y)
    {
        snake_body_node_s pSnakeNodesY[pSnakeGame->body_len];
        UINT32 iSnakeNodesYCount = 0;

        for(UINT32 i = 0; i < pSnakeGame->body_len; ++i)
        {
            if(pSnakeGame->body[i].y == y)
            {
                pSnakeNodesY[iSnakeNodesYCount] = pSnakeGame->body[i];
                ++iSnakeNodesYCount;
            }
        }

        for(UINT16 x = 0; x < pSnakeGame->border_width; ++x)
        {
            if(!x || !y || x == (pSnakeGame->border_width - 1) || y == (pSnakeGame->border_height - 1))
            {
                Print(SNAKE_BORDER_SYMBOL);
                continue;
            }
            
            if(pSnakeGame->fruit.x == x && pSnakeGame->fruit.y == y)
            {
                Print(SNAKE_FRUIT_SYMBOL);
                continue;
            }

            BOOLEAN bIsSnake = FALSE;
            for(UINT32 i = 0; i < iSnakeNodesYCount; ++i)
            {
                if(pSnakeNodesY[i].x == x)
                {
                    Print(SNAKE_BODY_SYMBOL);
                    
                    --iSnakeNodesYCount;
                    
                    bIsSnake = TRUE;
                    break;
                }
            }
            if(bIsSnake)
                continue;
            
            Print(L" ");
        }
        Print(L"\r\n");
    }
    Print(L"Score: %u\r\n", pSnakeGame->score);
    Print(L"ESC - Exit\r\nF1 - Restart\r\n");
}

void snake_tick(snake_game_s *const pSnakeGame)
{
    if(!pSnakeGame || !gST || !gBS)
        return;

    snake_draw(pSnakeGame);

    if(!snake_move(pSnakeGame))
    {
        snake_restart(pSnakeGame);
        return;    
    }

    if(pSnakeGame->body[0].x == pSnakeGame->fruit.x && pSnakeGame->body[0].y == pSnakeGame->fruit.y)
        snake_collect_fruit(pSnakeGame);

}

void snake_run(snake_game_s *const pSnakeGame)
{
    if(!gST || !gBS || !pSnakeGame)
        return;

    EFI_EVENT eTickEvent;
    gBS->CreateEvent(EVT_TIMER, TPL_NOTIFY, NULL, NULL, &eTickEvent);

    gBS->SetTimer(eTickEvent, TimerPeriodic, pSnakeGame->tick_rate_ms * 10000);

    pSnakeGame->running = TRUE;
    
    snake_tick(pSnakeGame); 
    while(pSnakeGame->running)
    {
        if(gBS->CheckEvent(eTickEvent) == EFI_SUCCESS)
            snake_tick(pSnakeGame);

        snake_listen_kb(pSnakeGame);
    }

    gBS->SetTimer(eTickEvent, TimerCancel, 0);
}

EFI_STATUS EFIAPI UefiEntry(IN EFI_HANDLE imgHandle, IN EFI_SYSTEM_TABLE* sysTable)
{
    gST = sysTable;
    gBS = sysTable->BootServices;
    gImageHandle = imgHandle;

    gBS->SetWatchdogTimer(0, 0, 0, NULL);

    const UINT64 iTickRate = 500;
    snake_game_s sSnakeGame = { 0 }; 
    
    if(!snake_init(&sSnakeGame, iTickRate))
        return EFI_NOT_STARTED;
    
    snake_run(&sSnakeGame);
    snake_clean(&sSnakeGame);

    return EFI_SUCCESS;
}