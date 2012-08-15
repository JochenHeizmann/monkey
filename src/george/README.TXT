
***** Markdown *****

George's minimal markdown support:

*this is in <em> tags*

**this is in <b> tags**

`This is in <code> tags`

%this is in <i> tags%

@param_identifier - uses <i> for now.

# This is an <h1>

## This is an <h2>

### This is an <h3>

[[This is a link]]

[[LinkTarget|This is a link to LinkTarget]]

[This is a link to LinkTarget](LinkTarget)

* This is a list item (must have whitespace after single * at start of line)

+ This is also a list item (must also have whitespace after single + at start of line)

<pre>
This is some preformatted text, ie: code!
Note: <pre> must appear at start of line, </pre> at end of line.
</pre>

\ can be used to escape any char, use for & and < and > to avoid html-ization.

2 or more spaces at end of line converts to a <br>.

Use html for everything else!
