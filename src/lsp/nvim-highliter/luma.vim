" luma.vim - Enhanced Syntax highlighting for Luma with custom colors
" Place this file at: ~/.config/nvim/syntax/luma.vim

if exists("b:current_syntax")
  finish
endif

" =====================
" COMMENTS
" =====================
syn match lumaComment "//.*$" contains=NONE containedin=ALL
syn region lumaComment start="/\*" end="\*/" contains=NONE containedin=ALL
hi def lumaComment guifg=#928374 gui=italic

" =====================
" PREPROCESSORS & ATTRIBUTES
" =====================
syn match lumaPreprocessor /@\w\+/
syn match lumaAttribute /#returns_ownership\|#takes_ownership/
hi def lumaPreprocessor guifg=#d3869b gui=bold
hi def lumaAttribute guifg=#8ec07c gui=bold,italic

" =====================
" KEYWORDS & TYPES
" =====================
syn keyword lumaKeyword const fn if elif else return loop let pub priv struct enum
syn keyword lumaType int float bool str char nil void short long
syn keyword lumaConditional if elif else
syn keyword lumaLoop loop
syn keyword lumaStatement return let
syn keyword lumaModifier const pub priv
syn keyword lumaBoolean true false

" =====================
" LITERALS
" =====================
syn match lumaNumber /\<\d\+\(\.\d\+\)\?\([eE][+-]\d\+\)\?/
syn region lumaString start=+"+ skip=+\\"+ end=+"+ contains=lumaEscape
syn region lumaString start=+'+ skip=+\\'+ end=+'+ contains=lumaEscape
syn match lumaEscape contained /\\[nrt\\'"]/

" =====================
" STRUCTS, ENUMS, FUNCTIONS
" =====================
syn match lumaStruct /\<\(struct\|enum\)\>/
syn match lumaFunction /\<fn\>\s\+\w\+\ze\s*[(]/ contains=lumaFunctionName
syn match lumaFunctionName /\w\+\ze\s*[(]/ contained

" =====================
" BUILT-IN FUNCTIONS
" =====================
syn keyword lumaBuiltinFunction output outputln alloc free sizeof cast

" =====================
" OPERATORS
" =====================
syn match lumaOperator /[+\-*/%=!<>&|]/
syn match lumaOperator /[(){}\[\],;:.]/
syn match lumaOperator /==/
syn match lumaOperator /!=/
syn match lumaOperator /<=/
syn match lumaOperator />=/
syn match lumaOperator /&&/
syn match lumaOperator /||/
syn match lumaOperator /++/
syn match lumaOperator /--/

" =====================
" TYPE ANNOTATIONS
" =====================
syn match lumaTypeAnnotation /:\s*\w\+/ contains=lumaType

" =====================
" VARIABLES
" =====================
syn match lumaVariable /\<[a-zA-Z_]\w*\>/ contains=NONE

" =====================
" COLORS (Gruvbox Palette)
" =====================
hi def lumaKeyword guifg=#fb4934 gui=bold
hi def lumaType guifg=#fabd2f gui=italic
hi def lumaConditional guifg=#fe8019 gui=bold
hi def lumaLoop guifg=#fe8019 gui=bold
hi def lumaStatement guifg=#83a598 gui=italic
hi def lumaModifier guifg=#d3869b gui=bold
hi def lumaBoolean guifg=#fb4934 gui=bold
hi def lumaNumber guifg=#d3869b
hi def lumaString guifg=#b8bb26
hi def lumaEscape guifg=#fe8019 gui=bold
hi def lumaOperator guifg=#fb4934
hi def lumaStruct guifg=#fabd2f gui=bold
hi def lumaFunction guifg=#8ec07c gui=bold
hi def lumaFunctionName guifg=#8ec07c gui=bold
hi def lumaBuiltinFunction guifg=#fe8019 gui=bold,italic
hi def lumaTypeAnnotation guifg=#fabd2f gui=italic
hi def lumaVariable guifg=#ebdbb2

let b:current_syntax = "luma"

