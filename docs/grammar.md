$$
\begin{align}
[\text{Module}] &\to \text{[Module-Statement]* EOF} \\
[\text{Module-Statement}] &\to
\begin{cases}
    \text{[Class]} \\
    \text{[Function]} \\
    \text{[Variable]} \\
    \text{[Branch]} \\
    \text{[Loop]} \\
    \text{[Expr]} \\
    \text{[NL]} \\
\end{cases} \\
[\text{Class-Statement}] &\to
\begin{cases}
    \text{[Function]} \\
    \text{[Variable]} \\
    \text{[NL]} \\
\end{cases} \\
[\text{Local-Statement}] &\to
\begin{cases}
    \text{[Class]} \\
    \text{[Function]} \\
    \text{[Variable]} \\
    \text{[Branch]} \\
    \text{[Loop]} \\
    \text{[Continue]} \\
    \text{[Break]} \\
    \text{[Return]} \\
    \text{[Expr]} \\
    \text{[NL]} \\
\end{cases} \\
\text{[Class]} &\to \{\text{static? class [Ident][NL] [Class-Statement]* [End]} \} \\
\text{[Function]} &\to \{\text{static? func [Ident] ([Variable]*) -> [Ident]? [Local-Statement]* [End]} \} \\
\text{[Variable]} &\to
\begin{cases}
    \text{let [Ident]: [Ident]? = [Expr][NL]} \\
    \text{const [Ident]: [Ident]? = [Expr][NL]}
\end{cases} \\
\text{[Branch]} &\to \{\text{else? if [Expr] then [Local-Statement]* [End]}\} \\
\text{[Loop]} &\to \{\text{while [Expr] do [Local-Statement]* [End]}\} \\
\text{[End]} &\to \{\text{end[NL]}\} \\
\text{[Continue]} &\to \{\text{continue}\} \\
\text{[Break]} &\to \{\text{break}\} \\
\text{[Return]} &\to \{\text{return}\} \\
[\text{Expr}] &\to
\begin{cases}
    [\text{LiteralExpr}] \\
    [\text{GroupExpr}] \\
    [\text{UnaryExpr}] \\
    [\text{BinaryExpr}] \\
\end{cases} \\
[\text{LiteralExpr}] &\to
\begin{cases}
    [\text{IntLiteral}] &\to
    \begin{cases}
        [\text{DecInt}] \\
        [\text{HexInt}] \\
        [\text{OctInt}] \\
        [\text{BinInt}] \\
    \end{cases} \\
    [\text{FloatLiteral}]  &\to
    \begin{cases}
        [\text{DecFloat}] \\
        [\text{HexFloat}] \\
        [\text{ScientificFloat}] \\
    \end{cases} \\
    [\text{BoolLiteral}] &\to
    \begin{cases}
        \text{true} \\
        \text{false} \\
    \end{cases} \\
    [\text{CharLiteral}] &\to
    \begin{cases}
        [\text{UTF32Char}] \\
        [\text{EscapedChar}] \\
    \end{cases} \\
    [\text{StringLiteral}] &\to
    \begin{cases}
        [\text{String}] \\
        [\text{FormatString}] \\
        [\text{RawString}] \\
    \end{cases} \\
     [\text{Self}] &\to \text{self} \\
\end{cases} \\
[\text{GroupExpr}] &\to \{\text{([Expr])}\} \\
[\text{UnaryExpr}] &\to
\begin{cases}
    + [\text{Expr}] \\
    - [\text{Expr}] \\
    \text{not} [\text{Expr}] \\
    \sim [\text{Expr}] \\
    ++ [\text{Expr}] \\
    -- [\text{Expr}] \\
    [\text{Expr}] ++ \\
    [\text{Expr}] -- \\
\end{cases} \\
[\text{BinExpr}] &\to 
\begin{cases} 
    [\text{Expr}] \text{ . } [\text{Expr}] \\
    [\text{Expr}] \text{ = } [\text{Expr}] \\
    [\text{Expr}] \text{ + } [\text{Expr}] \\
    [\text{Expr}] \text{ - } [\text{Expr}] \\
    [\text{Expr}] \text{ * } [\text{Expr}] \\
    [\text{Expr}] \text{ ** } [\text{Expr}] \\
    [\text{Expr}] \text{ !+ } [\text{Expr}] \\
    [\text{Expr}] \text{ !- } [\text{Expr}] \\
    [\text{Expr}] \text{ !* } [\text{Expr}] \\
    [\text{Expr}] \text{ !** } [\text{Expr}] \\
    [\text{Expr}] \text{ / } [\text{Expr}] \\
    [\text{Expr}] \text{ % } [\text{Expr}] \\
    [\text{Expr}] \text{ += } [\text{Expr}] \\
    [\text{Expr}] \text{ -= } [\text{Expr}] \\
    [\text{Expr}] \text{ *= } [\text{Expr}] \\
    [\text{Expr}] \text{ **= } [\text{Expr}] \\
    [\text{Expr}] \text{ !+= } [\text{Expr}] \\
    [\text{Expr}] \text{ !-= } [\text{Expr}] \\
    [\text{Expr}] \text{ !*= } [\text{Expr}] \\
    [\text{Expr}] \text{ !**= } [\text{Expr}] \\
    [\text{Expr}] \text{ /= } [\text{Expr}] \\
    [\text{Expr}] \text{ %= } [\text{Expr}] \\
    [\text{Expr}] \text{ == } [\text{Expr}] \\
    [\text{Expr}] \text{ < } [\text{Expr}] \\
    [\text{Expr}] \text{ > } [\text{Expr}] \\
    [\text{Expr}] \text{ <= } [\text{Expr}] \\
    [\text{Expr}] \text{ >= } [\text{Expr}] \\
    [\text{Expr}] \text{ & } [\text{Expr}] \\
    [\text{Expr}] \text{ | } [\text{Expr}] \\
    [\text{Expr}] \text{ \^ } [\text{Expr}] \\
    [\text{Expr}] \text{ &= } [\text{Expr}] \\
    [\text{Expr}] \text{ | }= [\text{Expr}] \\
    [\text{Expr}] \text{ \^= } [\text{Expr}] \\
    [\text{Expr}] \text{ << } [\text{Expr}] \\
    [\text{Expr}] \text{ >> } [\text{Expr}] \\
    [\text{Expr}] \text{ <<< } [\text{Expr}] \\
    [\text{Expr}] \text{ >>> } [\text{Expr}] \\
    [\text{Expr}] \text{ >>>> } [\text{Expr}] \\
    [\text{Expr}] \text{ <<= } [\text{Expr}] \\
    [\text{Expr}] \text{ >>= } [\text{Expr}] \\
    [\text{Expr}] \text{ <<<= } [\text{Expr}] \\
    [\text{Expr}] \text{ >>>= } [\text{Expr}] \\
    [\text{Expr}] \text{ >>>>= }[\text{Expr}] \\
    [\text{Expr}] \text{ and } [\text{Expr}] \\
    [\text{Expr}] \text{ or } [\text{Expr}] \\
\end{cases} \\
\text{[NL]} &\to \{\text{0xA} \} \\
\end{align}
$$
