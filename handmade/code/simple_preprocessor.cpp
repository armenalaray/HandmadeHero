/*
NOTE(Alex): We are gonna preprocess handmade_simulation_region.h to find all the elements \
in the struct sim_entity and parse it so we could add the same data to debug file  
*/

#include <windows.h>
#include <stdio.h>
#include "handmade_platform.h"
#include "simple_preprocessor.h"

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0,MEM_RELEASE);
    }
}


DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_file_content DebugFileContent ={};
    
    HANDLE FileHandle = CreateFileA(Filename,GENERIC_READ ,FILE_SHARE_READ, 0, OPEN_EXISTING, 0,0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        //Handle exists
        LARGE_INTEGER FileSize;
        
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            //TODO: Defines for maximum values UInt32Max
            uint32 FileSize32 = SafeTruncateToUint32(FileSize.QuadPart);
            
            DebugFileContent.Content = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if(DebugFileContent.Content)
            {
                DWORD BytesRead;
                if(ReadFile(FileHandle,DebugFileContent.Content,FileSize32, &BytesRead, 0) && 
                   (FileSize32 == BytesRead))
                {
                    DebugFileContent.ContentSize = FileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(DebugFileContent.Content);
                    DebugFileContent.Content = 0;
                }
            }
            else
            {
                //TODO:Logging
            }
        }
        else
        {
            //TODO:Logging
        }
        CloseHandle(FileHandle);
    }
    else
    {
        //TODO:Logging
    }
    return DebugFileContent;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 Result = false;
    HANDLE FileHandle = CreateFileA(Filename,GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0,0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if(WriteFile(FileHandle, Memory, MemorySize32, &BytesWritten, 0) && (MemorySize32 == BytesWritten))
        {
            Result = true;
        }
        else
        {
            Result = false;
        }
        CloseHandle(FileHandle);
    }
    else
    {
        //TODO:Logging
    }
    return Result;
}

//TODO(Alex): Try to unify this seraches?
internal b32 
TokenTextsAreEqual(token * TokenA, token * TokenB)
{
    b32 Result = false;
    if(TokenA && TokenB)
    {
        u32 Count = 0;
        char * A = TokenA->Text;
        char * B = TokenB->Text;
        
        if(TokenA->LetterCount == TokenB->LetterCount)
        {
            while(*A == *B && 
                  (Count < TokenA->LetterCount))
            {
                ++A; ++B;
                ++Count;
            }
            
            Result = (Count == TokenA->LetterCount);
        }
    }
    
    return Result;
}

//TODO(Alex): Assert there's a null terminator, on both strings
internal b32 
TokenTextIsEqualToString(meta_text * TextA, 
                         meta_text * TextB)
{
    b32 Result = false;
    u32 Count = 0;
    
    char * A = TextA->Text;
    char * B = TextB->Text;
    
    if(A && B)
    {
        if(TextA->Count == TextB->Count)
        {
            while(*A == *B)
            {
                ++A; ++B;
                Count++;
            }
            Result = (Count == TextA->Count);
        }
    }
    
    return Result;
}

inline b32
TokenIsIdentifier(token * Token, meta_text * MetaText)
{
    meta_text TokenText = {Token->LetterCount, Token->Text};
    
    b32 Result = ((Token->Type == TokenType_Identifier) && 
                  TokenTextIsEqualToString(&TokenText, MetaText));
    return Result;
}


inline b32
IsNewLine(char C)
{
    b32 Result = ((C == '\n') || (C == '\r'));
    return Result;
}

inline b32
CharIsWhiteSpace(char C)
{
    b32 Result = ((C == ' ') || 
                  (C == '\t') || 
                  (C == '\f') || 
                  (C == '\v') || 
                  IsNewLine(C));
    return Result;
}

inline b32
IsNumber(char C)
{
    b32 Result = ((C >= '0') && 
                  (C <= '9'));
    return Result;
}

inline b32
IsAlpha(char C)
{
    b32 Result = (((C >= 'A') && 
                   (C <= 'Z')) || ((C >= 'a') && 
                                   (C <= 'z')));
    return Result;
}


inline void
ParseCStyleComment(tokenizer * Tokenizer)
{
    Assert(Tokenizer->At);
    
    b32 SpecialReturn = false;
    Tokenizer->At += 2;
    while(Tokenizer->At[0])
    {
        if(!SpecialReturn)
        {
            SpecialReturn = ((Tokenizer->At[0] == '\\') && 
                             IsNewLine(Tokenizer->At[1]) && 
                             IsNewLine(Tokenizer->At[2]));
        }
        
        if(!SpecialReturn && 
           IsNewLine(Tokenizer->At[0]))
        {
            break;
        }
        else if(SpecialReturn && !IsNewLine(Tokenizer->At[1]))
        {
            SpecialReturn = false;
        }
        
        ++Tokenizer->At;
    }
}

inline void
ParseCppStyleComment(tokenizer * Tokenizer)
{
    Assert(Tokenizer->At);
    
    Tokenizer->At += 2;
    for(;;)
    {
        if((Tokenizer->At[0] == '*') &&
           (Tokenizer->At[1] == '/')) 
        {
            Tokenizer->At += 2;
            break;
        }
    }
}

//NOTE(Alex): we dont want to parse comments 
inline void
EliminateWhiteSpace(tokenizer * Tokenizer)
{
    for(;;)
    {
        if(CharIsWhiteSpace(Tokenizer->At[0]))
        {
            ++Tokenizer->At;
        }
        else if((Tokenizer->At[0] == '/') && 
                (Tokenizer->At[1] == '/'))
        {
            ParseCStyleComment(Tokenizer);
        }
        else if((Tokenizer->At[0] == '/') && 
                (Tokenizer->At[1] == '/*'))
        {
            ParseCppStyleComment(Tokenizer);
        }
        else
        {
            break;
        }
    }
}



//NOTE(Alex): Make a hashtable of tokens so we can access them faster?
internal token
GetToken(tokenizer * Tokenizer, b32 CheckToken = false)
{
    Assert(Tokenizer->At < ((char*)Tokenizer->File->Content + Tokenizer->File->ContentSize));
    EliminateWhiteSpace(Tokenizer);
    
    token Result = {};
    Result.Text = Tokenizer->At;
    Result.LetterCount = 1;
    
    switch (Tokenizer->At[0])
    {
        case '\0':{Result.Type = TokenType_EndOfStream;}break;
        case '(':{Result.Type = TokenType_OpenParen;}break;
        case ')':{Result.Type = TokenType_CloseParen;}break;
        case ':':{Result.Type = TokenType_Colon;}break;
        case ';':{Result.Type = TokenType_SemiColon;}break;
        case '[':{Result.Type = TokenType_OpenBracket;}break;
        case ']':{Result.Type = TokenType_CloseBracket;}break;
        case '{':{Result.Type = TokenType_OpenBraces;}break;
        case '}':{Result.Type = TokenType_CloseBraces;}break;
        case '*':{Result.Type = TokenType_Asterisk;}break;
        case '&':{Result.Type = TokenType_Ampersand;}break;
        case '\"':
        {
            ++Tokenizer->At;
            Result.Text = Tokenizer->At;
            Result.Type = TokenType_String;
            
            while(Tokenizer->At[0] && 
                  (Tokenizer->At[0] != '"'))
            {
                if((Tokenizer->At[0] == '\\') && 
                   Tokenizer->At[1])
                {
                    ++Tokenizer->At;
                }
                
                ++Tokenizer->At;
            }
            
            Result.LetterCount = SafeTruncateToUint32(Tokenizer->At - Result.Text);
            
            if(Tokenizer->At[0] == '"')
            {
                ++Tokenizer->At;
            }
            
        }break;
        default:
        {
            if(IsAlpha(Tokenizer->At[0]))
            {
                Result.Type = TokenType_Identifier;
                while(IsAlpha(Tokenizer->At[0]) || 
                      IsNumber(Tokenizer->At[0]) ||
                      Tokenizer->At[0] == '_')
                {
                    ++Tokenizer->At;
                }
                
                Result.LetterCount = SafeTruncateToUint32(Tokenizer->At - Result.Text); 
            }
            else if(IsNumber(Tokenizer->At[0]))
            {
                Result.Type = TokenType_Number;
                //TODO(Alex): Expand this to other numeric types
                while(IsNumber(Tokenizer->At[0]) ||
                      Tokenizer->At[0] == '.' || 
                      Tokenizer->At[0] == 'f')
                {
                    ++Tokenizer->At;
                }
                
                Result.LetterCount = SafeTruncateToUint32(Tokenizer->At - Result.Text); 
            }
            else
            {
                //NOTE(Alex): unknown token
                Result.Type = TokenType_Unknown;
            }
        }break;
    }
    
    if(Result.Type >= TokenType_Unknown)
    {
        ++Tokenizer->At;
    }
    
    if(CheckToken)
    {
        Tokenizer->At = Result.Text;
    }
    
    return Result;
}

inline void
TokenAdvance(tokenizer * Tokenizer, token * Token)
{
    Tokenizer->At += Token->LetterCount; 
}

inline token
CheckNextToken(tokenizer * Tokenizer)
{
    token Result = GetToken(Tokenizer, true);
    return Result;
}

struct token_result
{
    b32 IsOfType;
    token Token;
};

inline token_result
CheckForTokenType(tokenizer * Tokenizer, token_type Type)
{
    token_result Result = {};
    Result.Token= GetToken(Tokenizer);
    Result.IsOfType = (Result.Token.Type == Type);
    
    return Result;
}

internal b32
CheckValidIntrospection(tokenizer * Tokenizer)
{
    b32 Result = 0;
    if(CheckForTokenType(Tokenizer, TokenType_OpenParen).IsOfType)
    {
        Result = true;
        for(;;)
        {
            token IntroParam = GetToken(Tokenizer);
            if(IntroParam.Type == TokenType_CloseParen)
            {
                break;
            }
        }
    }
    else
    {
        printf("ERROR: Invalid Introspection Params");
    }
    
    return Result;
}

inline b32
CheckForKeyword(token * Token)
{
    meta_text TokenText = {Token->LetterCount, Token->Text};
    b32 Result = (TokenTextIsEqualToString(&TokenText, &(MetaText(StringLength("static"), "static"))) || 
                  TokenTextIsEqualToString(&TokenText, &MetaText(StringLength("volatile"), "volatile")) ||
                  TokenTextIsEqualToString(&TokenText, &MetaText(StringLength("public"), "public")) ||
                  TokenTextIsEqualToString(&TokenText, &MetaText(StringLength("private"),"private" )) ||
                  TokenTextIsEqualToString(&TokenText, &MetaText(StringLength("extern"), "extern"))||
                  TokenTextIsEqualToString(&TokenText, &MetaText(StringLength("const"), "const")) ||
                  TokenTextIsEqualToString(&TokenText, &MetaText(StringLength("friend"), "friend")) ||
                  TokenTextIsEqualToString(&TokenText, &MetaText(StringLength("struct"), "struct")) ||
                  TokenTextIsEqualToString(&TokenText, &MetaText(StringLength("virtual"), "virtual")));
    
    return Result;
}

inline void
ConcatFormattedText(output_text * OutputText, u32 TextCount, char * Text)
{
    ConcatStringsA(OutputText->Count, 
                   OutputText->Buffer,
                   TextCount, Text,
                   ArrayCount(OutputText->Buffer), OutputText->Buffer);
    
    OutputText->Count += TextCount;
}

//TODO(Alex): probably we could expand AddFormattedChunkToBuffer to have a more general format expansion 
internal void
AddFormattedChunkToBuffer(output_text * OutputText, char * Format, token * Token = 0)
{
    char Text[MAX_BUFFER_SIZE] = {0};
    u32 CharsWritten = 0;
    if(Token)
    {
        CharsWritten = _snprintf_s(Text, sizeof(Text),
                                   Format, 
                                   Token->LetterCount, 
                                   Token->Text);
    }
    else
    {
        CharsWritten = _snprintf_s(Text, sizeof(Text), Format); 
    }
    
    ConcatFormattedText(OutputText, CharsWritten, Text);
}

internal void
GetHashedToken(token * Other, u32 Flags)
{
    //TODO(Alex): Better Hash Function
    u32 Index = ((13 * Other->Type) + (7 * (u32)Other->Text[0])) & (ArrayCount(SeenTokens.Hash) - 1);
    
    hashed_token * Temp = SeenTokens.Hash[Index];
    for(;
        Temp;
        Temp = Temp->NextInHash)
    {
        if(TokenTextsAreEqual(&Temp->Token, Other))
        {
            break;
        }
    }
    
    if(!Temp)
    {
        //NOTE(Alex): Alloc new token seen
        Temp = (hashed_token*)malloc(sizeof(hashed_token));
        
        Temp->Flags = Flags;
        Temp->Token = *Other;
        Temp->NextInHash = SeenTokens.Hash[Index];
        SeenTokens.Hash[Index] = Temp; 
    }
}

inline b32 
CheckForType(tokenizer * Tokenizer, output_text * OutputText, token Token)
{
    b32 Result = false;
    if(CheckForKeyword(&Token))
    {
    }
    else
    {
        Result = true;
        AddFormattedChunkToBuffer(OutputText,"{MetaType_%.*s", &Token);
        
        GetHashedToken(&Token, TokenFlag_MemberType);
        TokenAdvance(Tokenizer, &Token);
    }
    
    return Result;
}


#if 0        
char TextBuffer[256] = {0};
u32 CharsWritten = _snprintf_s(TextBuffer, sizeof(TextBuffer),
                               "MetaType_%.*s", 
                               Token.LetterCount, 
                               Token.Text);

meta_member_type * Temp  = (meta_member_type*)malloc(sizeof(*FirstMetaMemberType));
Temp->Name = (char*)malloc(CharsWritten + 1);
Copy(CharsWritten, TextBuffer, Temp->Name);

Temp->Name[CharsWritten] = 0;

Temp->Next = FirstMetaMemberType;
FirstMetaMemberType = Temp;
#endif


inline void
CheckForPointerReference(tokenizer * Tokenizer, output_text * OutputText)
{
    token Token = CheckNextToken(Tokenizer);
    if(Token.Type == TokenType_Asterisk)
    {
        ConcatFormattedText(OutputText, 5, "Ptr, ");
        TokenAdvance(Tokenizer, &Token);
    }
    else
    {
        if(Token.Type == TokenType_Ampersand)
        {
            TokenAdvance(Tokenizer, &Token);
        }
        ConcatFormattedText(OutputText, 5, ",    ");
    }
}


inline void
CheckForName(tokenizer * Tokenizer, output_text * OutputText, token * StructName)
{
    token Token = GetToken(Tokenizer);
    
    AddFormattedChunkToBuffer(OutputText,"\"%.*s\", ", &Token);
    
    AddFormattedChunkToBuffer(OutputText, "(memory_index)&(((%.*s *)(0))->", StructName);
    AddFormattedChunkToBuffer(OutputText, "%.*s), ", &Token);
    AddFormattedChunkToBuffer(OutputText,"sizeof(((%.*s *)0)->", StructName);
    AddFormattedChunkToBuffer(OutputText, "%.*s), ", &Token);
}

inline void
CheckForArray(tokenizer * Tokenizer, output_text * OutputText)
{
    token ArrayValue = {};
    ArrayValue.Type = TokenType_Number;
    ArrayValue.LetterCount = 1;
    ArrayValue.Text = "1";
    
    token Token = CheckNextToken(Tokenizer);
    if(Token.Type == TokenType_OpenBracket)
    {
        TokenAdvance(Tokenizer, &Token);
        
        Token = CheckNextToken(Tokenizer);
        if(Token.Type == TokenType_Identifier || 
           Token.Type == TokenType_Number)
        {
            ArrayValue = Token;
            TokenAdvance(Tokenizer, &Token);
            
            Token = CheckNextToken(Tokenizer);
        }
        
        TokenAdvance(Tokenizer, &Token);
    }
    
    AddFormattedChunkToBuffer(OutputText, "%.*s", &ArrayValue);
}

inline token
CheckForEnd(tokenizer * Tokenizer, output_text * OutputText)
{
    token Result = {};
    for(;;)
    {
        Result = CheckNextToken(Tokenizer);
        if(Result.Type == TokenType_SemiColon)
        {
            //TokenAdvance(Tokenizer, &Token);
            
            char Text[] = "},\n";
            ConcatFormattedText(OutputText, ArrayCount(Text) - 1, Text);
            break;
        }
    }
    
    return Result;
}


internal void
ParseStructMembers(tokenizer * Tokenizer, token * NameStruct)
{
    output_text OutputText = {};
    b32 EndOfStruct = false;
    for(;
        !EndOfStruct;
        )
    {
        token Token = CheckNextToken(Tokenizer);
        switch(Token.Type)
        {
            case TokenType_CloseBraces:
            {
                --Tokenizer->StackLevel;
                if(Tokenizer->StackLevel == 0)
                {
                    EndOfStruct = true;
                }
            }break;
            case TokenType_OpenBraces:
            {
                ++Tokenizer->StackLevel;
            }break;
            case TokenType_Identifier:
            {
                //TODO(Alex): CheckForValue(Tokenizer);?
                if(CheckForType(Tokenizer, &OutputText, Token))
                {
                    CheckForPointerReference(Tokenizer, &OutputText);
                    CheckForName(Tokenizer, &OutputText, NameStruct);
                    CheckForArray(Tokenizer, &OutputText);
                    Token = CheckForEnd(Tokenizer, &OutputText);
                }
            };
            default:
            {
            }break;
        }
        
        TokenAdvance(Tokenizer, &Token);
    }
    
    printf(OutputText.Buffer);
}

internal void
ParseStruct(tokenizer * Tokenizer)
{
    token StructName = GetToken(Tokenizer);
    if(CheckForTokenType(Tokenizer, TokenType_OpenBraces).IsOfType)
    {
        ++Tokenizer->StackLevel;
        
        GetHashedToken(&StructName, TokenFlag_IntrospectedStruct);
        printf("member_definition DefinitionOf_%.*s [] =\r\n{\r\n", StructName.LetterCount, StructName.Text);
        
        ParseStructMembers(Tokenizer, &StructName);
        printf("};\n");
    }
    else
    {
        printf("ERROR: Not definition of struct");
    }
}

internal void
ParseIntrospection(tokenizer * Tokenizer)
{
    if(CheckValidIntrospection(Tokenizer))
    {
        token Token = GetToken(Tokenizer);
        
        if(TokenIsIdentifier(&Token, &MetaText(StringLength("struct"), "struct")) ||
           TokenIsIdentifier(&Token, &MetaText(StringLength("union"), "union")))
        {
            ParseStruct(Tokenizer);
        }
    }
}


internal void 
LexFile(char * FileName)
{
    debug_file_content File = DEBUGPlatformReadEntireFile(FileName);
    
    tokenizer Tokenizer = {};
    Tokenizer.Tokenizing = false;
    Tokenizer.File = &File;
    Tokenizer.At = (char*)File.Content;
    
    for(;
        Tokenizer.At < ((char*)File.Content + File.ContentSize);
        )
    {
        token Token = GetToken(&Tokenizer);
        switch(Token.Type)
        {
            default:
            {
                
            }break;
            case TokenType_Identifier:
            {
                if(TokenIsIdentifier(&Token, &MetaText(StringLength("Introspection"), "Introspection")))
                {
                    ParseIntrospection(&Tokenizer);
                }
            }break;
            case TokenType_Unknown:
            {
            }
            case TokenType_EndOfStream:
            {
                break;
            }break;
        }
    }
}

int 
main(int ArgCount, char **Args)
{
    LexFile("handmade_simulation_region.h");
    LexFile("handmade_math.h");
    LexFile("handmade_platform.h");
    
    
    //TODO(Alex): Implement this?
    //GetHashedTokenWithFlag(TokenFlag_IntrospectedStruct);
    //AddFormattedChunkToBuffer(&OutputText, "#define Introspected_Struct_List()\\\nenum introspected_struct_list\\\n{\\\n");
    printf("#define DEBUG_SWITCH_MEMBER_DEFS(DepthLevel)\\\n");
    
    for(u32 BucketIndex = 0;
        BucketIndex < ArrayCount(SeenTokens.Hash);
        ++BucketIndex)
    {
        hashed_token * Hashed = SeenTokens.Hash[BucketIndex];
        for(;
            Hashed;
            Hashed = Hashed->NextInHash)
        {
            if(Hashed->Flags & TokenFlag_IntrospectedStruct)
            {
                //NOTE(Alex): Debug switch members
                printf("case MetaType_%.*s:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, \"%s:\", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_%.*s), DefinitionOf_%.*s, MemberOffset, DepthLevel);}break;",
                       Hashed->Token.LetterCount, Hashed->Token.Text,
                       "%s",
                       Hashed->Token.LetterCount, Hashed->Token.Text,
                       Hashed->Token.LetterCount, Hashed->Token.Text);
                
                printf("case MetaType_%.*sPtr:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, \"%s:\", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_%.*s), DefinitionOf_%.*s, (void*)(*(u64*)MemberOffset), DepthLevel);}break;", 
                       Hashed->Token.LetterCount, Hashed->Token.Text,
                       "%s",
                       Hashed->Token.LetterCount, Hashed->Token.Text,
                       Hashed->Token.LetterCount, Hashed->Token.Text);
                
                //AddFormattedChunkToBuffer(&OutputText, "IntroStruct_%.*s,\\\n", &Hashed->Token);
            }
        }
    }
    //AddFormattedChunkToBuffer(&OutputText, "IntroStruct_Count,\\\n};\n\n");
    
    
    output_text OutputText = {};
    AddFormattedChunkToBuffer(&OutputText, "#define META_MEMBER_TYPE_ENUM()\\\nenum member_definition_type\\\n{\\\n");
    for(u32 BucketIndex = 0;
        BucketIndex < ArrayCount(SeenTokens.Hash);
        ++BucketIndex)
    {
        hashed_token * Hashed = SeenTokens.Hash[BucketIndex];
        for(;
            Hashed;
            Hashed = Hashed->NextInHash)
        {
            AddFormattedChunkToBuffer(&OutputText, "MetaType_%.*s,\\\n", &Hashed->Token);
            AddFormattedChunkToBuffer(&OutputText, "MetaType_%.*sPtr,\\\n", &Hashed->Token);
        }
    }
    AddFormattedChunkToBuffer(&OutputText, "};\n");
    
    DEBUGPlatformWriteEntireFile("handmade_meta_type_generated.h", OutputText.Count, OutputText.Buffer);
    
    
    printf("\0");
    return 0;
}





























