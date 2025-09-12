" l.vim - Enhanced Syntax highlighting for Lux with custom colors
" Place this file at: ~/.config/syntax/l.lua

if exists("b:current_syntax")
  finish
endif

" =====================
" COMMENTS (Top Priority)
" =====================
syn match luxComment "::.*$" contains=NONE containedin=ALL
syn region luxComment start="/\*" end="\*/" contains=NONE containedin=ALL
hi def luxComment guifg=#928374 gui=italic

" =====================
" PREPROCESSORS
" =====================
syn match luxPreprocessor /@\w\+/
hi def luxPreprocessor guifg=#d3869b gui=bold

" =====================
" KEYWORDS & TYPES
" =====================
syn keyword luxKeyword const fn if elif else return loop let pub priv struct enum
syn keyword luxType int float bool str char nil void short long
syn keyword luxConditional if elif else
syn keyword luxLoop loop
syn keyword luxStatement return let
syn keyword luxModifier const pub priv
syn keyword luxBoolean true false

" =====================
" LITERALS
" =====================
syn match luxNumber /\<\d\+\(\.\d\+\)\?\([eE][+-]\d\+\)\?/
syn region luxString start=+"+ skip=+\\"+ end=+"+ contains=luxEscape
syn region luxString start=+'+ skip=+\\'+ end=+'+ contains=luxEscape
syn match luxEscape contained /\\[nrt\\'"]/

" =====================
" STRUCTS, ENUMS, FUNCTIONS
" =====================
syn match luxStruct /\<\(struct\|enum\)\>/
syn match luxFunction /\<fn\>\s\+\w\+\ze\s*[(]/ contains=luxFunctionName
syn match luxFunctionName /\w\+\ze\s*[(]/ contained

" =====================
" BUILT-IN FUNCTIONS
" =====================
syn keyword luxBuiltinFunction output outputln alloc free sizeof cast memcpy

" =====================
" OPERATORS
" =====================
syn match luxOperator /[+\-*/%=!<>&|]/
syn match luxOperator /[(){}\[\],;:.]/
syn match luxOperator /==/
syn match luxOperator /!=/
syn match luxOperator /<=/
syn match luxOperator />=/
syn match luxOperator /&&/
syn match luxOperator /||/
syn match luxOperator /++/
syn match luxOperator /--/

" =====================
" TYPE ANNOTATIONS
" =====================
syn match luxTypeAnnotation /:\s*\w\+/ contains=luxType

" =====================
" VARIABLES
" =====================
syn match luxVariable /\<[a-zA-Z_]\w*\>/ contains=NONE

" =====================
" COLORS (Gruvbox Palette)
" =====================
hi def luxKeyword guifg=#fb4934 gui=bold
hi def luxType guifg=#fabd2f gui=italic
hi def luxConditional guifg=#fe8019 gui=bold
hi def luxLoop guifg=#fe8019 gui=bold
hi def luxStatement guifg=#83a598 gui=italic
hi def luxModifier guifg=#d3869b gui=bold
hi def luxBoolean guifg=#fb4934 gui=bold
hi def luxNumber guifg=#d3869b
hi def luxString guifg=#b8bb26
hi def luxEscape guifg=#fe8019 gui=bold
hi def luxOperator guifg=#fb4934
hi def luxStruct guifg=#fabd2f gui=bold
hi def luxFunction guifg=#8ec07c gui=bold
hi def luxFunctionName guifg=#8ec07c gui=bold
hi def luxBuiltinFunction guifg=#fe8019 gui=bold,italic
hi def luxTypeAnnotation guifg=#fabd2f gui=italic
hi def luxVariable guifg=#ebdbb2

let b:current_syntax = "lux"


