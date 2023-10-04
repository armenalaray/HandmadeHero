#ifndef SIMPlE_PREPROCESSOR_H

#define MAX_BUFFER_SIZE 4096
#define STRUCT_DEF_SIZE 256

enum token_type
{
    Tokentype_Null,
    TokenType_Identifier,
    TokenType_Number,
    TokenType_String,
    
    TokenType_Unknown,
    TokenType_Equal,
    TokenType_OpenParen,
    TokenType_CloseParen,
    TokenType_Colon,
    TokenType_SemiColon,
    TokenType_OpenBracket,
    TokenType_CloseBracket,
    TokenType_OpenBraces,
    TokenType_CloseBraces,
    TokenType_Asterisk,
    TokenType_Ampersand,
    TokenType_EndOfStream,
};

//TODO(Alex): Advanze on this?
enum member_struct_type
{
    Member_Null,
    Member_Type,
    Member_Keyword,
    Member_Pointer,
    Member_Reference,
    Member_Name,
    Member_Array,
    Member_Value,
    Member_Equal,
    Member_End,
};

struct meta_text
{
    u32 Count;
    char * Text;
};

struct struct_def
{
    char * Name;
};

struct meta_struct
{
    u32 Count;
    struct_def StructDefs[4096];
};


struct meta_member_type
{
    char * Name;
    meta_member_type* Next;
};

//NOTE(Alex): this are not cached right now. we coult cache it

struct token
{
    token_type Type;
    u32 LetterCount;
    char * Text;
};

struct output_text
{
    u32 Count;
    char Buffer[MAX_BUFFER_SIZE];
};


enum identifiers_found
{
    IDFound_Null,
    IDFound_Keyword,
    IDFound_Type,
    IDFound_Name,
};


struct tokenizer
{
    debug_file_content * File;
    b32 Tokenizing;
    char * At;
    
    u32 IDFound;
    u32 StackLevel;
};

struct hashed_token
{
    token Token;
    u32 Flags;
    
    hashed_token * NextInHash;
};

struct seen_tokens
{
    hashed_token * Hash[512];
};

enum token_flags
{
    TokenFlag_Null = (0),
    TokenFlag_IntrospectedStruct = (1 << 0),
    TokenFlag_MemberType = (1 << 1),
};

//TODO(Alex): Probably this shows the need of a state presence
global_variable seen_tokens SeenTokens = {};
//global_variable meta_member_type * FirstMetaMemberType = 0;
//global_variable meta_struct MetaStruct = {};


inline meta_text
MetaText(u32 Count, char * String)
{
    meta_text Result = {Count, String};
    return Result;
}

#define SIMPLE_PREPROCESSOR_H
#endif

