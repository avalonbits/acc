/* Digraphs, C99 6.4.6: <: :> <% %> %: %:%: are [ ] { } # ##, as tokens.
 *
 * Each is a token and not a text substitution, so the "<:" inside a string
 * stays what it is written as. %: is a directive first on its line, also in
 * a group an #if is skipping, and in a macro body %: quotes its argument and
 * %:%: joins two. */
%:define PASTE(a, b) a %:%: b
%:define QUOTE(x) %:x
%:define TEN 10

%:if 0
%:error this group is skipped
%:endif

static int sum(int *v, int n) <%
    int s = 0, i;

    for (i = 0; i < n; i++)
        s += v<:i:>;
    return s;
%>

int main(void) <%
    int r = 0;
    int v<:3:> = <% 1, 2, 3 %>;
    int ab = 7;
    const char *s = "<:%>";
    const char *q = QUOTE(hi);

    if (sum(v, 3) == 6) r++;
    if (PASTE(a, b) == 7) r++;
    if (TEN == 10) r++;
    if (s<:0:> == '<' && s<:1:> == ':' && s<:3:> == '>') r++;
    if (q<:0:> == 'h' && q<:2:> == 0) r++;
    ab %= 4;                            /* %= and % and ?: are themselves */
    if (ab % 2 == 1 && (ab <3 ? 0 : ab) == 3) r++;

    return r + 36;              /* 6 checks */
%>
