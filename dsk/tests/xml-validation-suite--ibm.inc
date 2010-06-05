/* --- these tests code from the IBM portion of 
       Extensible Markup Language (XML) Conformance Test Suites
       http://www.w3.org/XML/Test/
   --- */
{
  "ibm01v01.xml",
  565,
  "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\" ?>\r\n<!-- Above is XMLDecl -->\r\n<!DOCTYPE animal [\r\n<!ELEMENT animal (cat|tiger|leopard)+>\r\n<!ELEMENT cat EMPTY>\r\n<!ELEMENT tiger (#PCDATA)>\r\n<!ELEMENT leopard ANY>\r\n<!ELEMENT small EMPTY>\r\n<!ELEMENT big EMPTY>\r\n<!ATTLIST tiger color CDATA #REQUIRED>\r\n]>\r\n<!-- Above is DTD -->\r\n<?music \"Here is a PI\" ?> \r\n<animal>\r\n   <cat/>\r\n   <tiger color=\"white\">This is a white tiger in Mirage!!</tiger>\r\n   <cat/>\r\n   <leopard>\r\n      <small/>\r\n      <big/>\r\n   </leopard>\r\n</animal>\r\n<!-- Above is element animal -->\r\n\r\n",
  238,
  "<?music \"Here is a PI\" ?><animal>&#10;   <cat></cat>&#10;   <tiger color=\"white\">This is a white tiger in Mirage!!</tiger>&#10;   <cat></cat>&#10;   <leopard>&#10;      <small></small>&#10;      <big></big>&#10;   </leopard>&#10;</animal>"
},
{
  "ibm02v01.xml",
  345,
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n<!DOCTYPE book [\r\n<!ELEMENT book ANY>\r\n<!-- This test case covers     legal character ranges plus\r\n        discrete legal characters for production 02. -->\r\n<?NAME_09-\t_0A-\r\n_0D-\r\n_20- _D7FF-\355\237\277\0666c0f-\346\260\217\137\060000-\356\200\200\137\106\106\106\104\55\357\277\275\137\145\146\146\145\55\356\277\276\060010000-\360\220\200\200\06110FFFF-\364\217\277\277\06008ffff-\362\217\277\277 This is a PI target ?>\r\n]>\r\n<book/>\r\n",
  145,
  "<?NAME_09- _0A-\n\n_0D-\n_20- _D7FF-\355\237\277\0666c0f-\346\260\217\137\060000-\356\200\200\137\106\106\106\104\55\357\277\275\137\145\146\146\145\55\356\277\276\060010000-\360\220\200\200\06110FFFF-\364\217\277\277\06008ffff-\362\217\277\277 This is a PI target ?><book></book>\n"
},
{
  "ibm03v01.xml",
  269,
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\r\n<!DOCTYPE book [\r\r\n<!ELEMENT book ANY>\r\r\n<!-- This test case covers 0 legal character ranges plus\r\r\n     4 discrete legal characters for production 03. -->\r\r\n<?NAME_20- _09-\t_0D-\r_0A-\r\r\n This is a PI target ?>\r\r\n]>\r\r\n<book/>\r\r\n",
  63,
  "<?NAME_20- _09-\t_0D-\n_0A-\n\n This is a PI target ?><book></book>"
},
{
  "ibm09v01.xml",
  178,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n \t<!ENTITY FullName \"\">\r\n]>\r\n\r\n<student>My Name is &FullName;. </student>\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n ",
  32,
  "<student>My Name is . </student>"
},
{
  "ibm09v02.xml",
  156,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n \t<!ENTITY FullName \"SnowMan\">\r\n]>\r\n\r\n<student>My Name is &FullName;. </student>",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm09v03.xml",
  117,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student  SYSTEM \"ibm09v03.dtd\">\r\n<student>I am a new student with &Name;</student>\r\n",
  55,
  "<student>I am a new student with first , last</student>"
},
{
  "ibm09v04.xml",
  237,
  "<?xml version=\"1.0\"?> \r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n<!-- testing entity value with Reference -->\r\n\t<!ENTITY RealName \"SnowMan\"> \r\n \t<!ENTITY FullName \"&RealName;\">\r\n]>\r\n\r\n<student>My Name is &FullName;. </student>",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm09v05.xml",
  313,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student SYSTEM \"student.dtd\"[\r\n\t<!ELEMENT student (#PCDATA)> \r\n\t<!ENTITY Age \"21\">\r\n\t<!ENTITY Status \"freshman\">\r\n \t<!ENTITY % FullName \"first , last , middle\">\r\n]>\r\n\r\n<!-- testing entity value with combination reference -->\r\n<student>This is a test of &combine;</student>\r\n\r\n\r\n\r\n",
  257,
  "<student>This is a test of My Name is first , last , middle and my age is 21 Again first , last , middle first , last , middle and my status is &#10;&#9;&#9;freshman freshman and first , last , middle 21 first , last , middle freshman That is all.</student>"
},
{
  "ibm10v01.xml",
  393,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student\r\n\t\tfirst CDATA #REQUIRED\r\n\t\tmiddle CDATA #IMPLIED\r\n\t\tlast CDATA #REQUIRED > \r\n\t<!ENTITY myfirst \"Snow\">\r\n\t<!ENTITY mymiddle \"Y\">\r\n\t<!ENTITY mylast \"\">\r\n]>\r\n<!-- testing AttValue with empty char inside double quote -->\r\n<student first=\"\" last=\"\">My Name is Snow &mylast; Man. </student>\r\n\r\n\r\n\r\n\r\n\r\n\r\n",
  58,
  "<student first=\"\" last=\"\">My Name is Snow  Man. </student>"
},
{
  "ibm10v02.xml",
  383,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student\r\n\t\tfirst CDATA #REQUIRED\r\n\t\tmiddle CDATA #IMPLIED\r\n\t\tlast CDATA #REQUIRED > \r\n\t<!ENTITY myfirst \"Snow\">\r\n\t<!ENTITY mymiddle \"Y\">\r\n\t<!ENTITY mylast ''>\r\n]>\r\n<!-- testing AttValue with empty char inside single quote -->\r\n<student first='' last=''>My Name is Snow &mylast; Man. </student>\r\n\r\n",
  58,
  "<student first=\"\" last=\"\">My Name is Snow  Man. </student>"
},
{
  "ibm10v03.xml",
  390,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student\r\n\t\tfirst CDATA #REQUIRED\r\n\t\tmiddle CDATA #IMPLIED\r\n\t\tlast CDATA #REQUIRED > \r\n\t<!ENTITY myfirst 'Snow'>\r\n\t<!ENTITY mymiddle 'I'>\r\n\t<!ENTITY mylast \"Man'\">\r\n]>\r\n<!-- testing AttValue string with a single quote inside -->\r\n<student first=\"Snow'\" last=\"Man\">My Name is &myfirst; &mylast;. </student>",
  66,
  "<student first=\"Snow'\" last=\"Man\">My Name is Snow Man'. </student>"
},
{
  "ibm10v04.xml",
  394,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student\r\n\t\tfirst CDATA #REQUIRED\r\n\t\tmiddle CDATA #IMPLIED\r\n\t\tlast CDATA #REQUIRED > \r\n\t<!ENTITY myfirst 'Snow'>\r\n\t<!ENTITY mymiddle 'I'>\r\n\t<!ENTITY mylast 'Man\"'>\r\n]>\r\n<!-- testing AttValue string with a double quote inside -->\r\n<student first='Snow\"' last='Man'>My Name is &myfirst; &mylast;. </student>\r\n\r\n",
  76,
  "<student first=\"Snow&quot;\" last=\"Man\">My Name is Snow Man&quot;. </student>"
},
{
  "ibm10v05.xml",
  400,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student\r\n\t\tfirst CDATA #REQUIRED\r\n\t\tmiddle CDATA #IMPLIED\r\n\t\tlast CDATA #REQUIRED > \r\n\t<!ENTITY myfirst \"Snow\">\r\n\t<!ENTITY mymiddle \"Y\">\r\n\t<!ENTITY mylast \"&myfirst; Man\">\r\n]>\r\n<!-- testing AttValue with a reference in double quote -->\r\n<student first=\"&myfirst;\" last=\"mylast;\">My Name is &mylast;. </student>\r\n\r\n",
  68,
  "<student first=\"Snow\" last=\"mylast;\">My Name is Snow Man. </student>"
},
{
  "ibm10v06.xml",
  401,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student\r\n\t\tfirst CDATA #REQUIRED\r\n\t\tmiddle CDATA #IMPLIED\r\n\t\tlast CDATA #REQUIRED > \r\n\t<!ENTITY myfirst \"Snow\">\r\n\t<!ENTITY mymiddle \"Y\">\r\n\t<!ENTITY mylast '&myfirst; Man'>\r\n]>\r\n<!-- testing AttValue with a reference in single quote -->\r\n<student first='&myfirst;' last='&mylast;'>My Name is &mylast;. </student>\r\n\r\n",
  69,
  "<student first=\"Snow\" last=\"Snow Man\">My Name is Snow Man. </student>"
},
{
  "ibm10v07.xml",
  519,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student\r\n\t\tfirst CDATA #REQUIRED\r\n\t\tmiddle CDATA #IMPLIED\r\n\t\tlast CDATA #REQUIRED > \r\n\t<!ENTITY myfirst \"Snow\">\r\n\t<!ENTITY mymiddle \"Y\">\r\n\t<!ENTITY mylast \"Man &myfirst; and &myfirst; mymiddle;.\">\r\n]>\r\n<!-- testing AttValue with references combination in double quotes -->\r\n<student first=\"Full Name &myfirst; &#x31; and &mylast; &mylast; &#x63;\" last=\"&mylast;\" >My first Name is &myfirst; and my last name is &mylast;. </student>\r\n",
  216,
  "<student first=\"Full Name Snow 1 and Man Snow and Snow mymiddle;. Man Snow and Snow mymiddle;. c\" last=\"Man Snow and Snow mymiddle;.\">My first Name is Snow and my last name is Man Snow and Snow mymiddle;.. </student>"
},
{
  "ibm10v08.xml",
  517,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student\r\n\t\tfirst CDATA #REQUIRED\r\n\t\tmiddle CDATA #IMPLIED\r\n\t\tlast CDATA #REQUIRED > \r\n\t<!ENTITY myfirst \"Snow\">\r\n\t<!ENTITY mymiddle \"I\">\r\n\t<!ENTITY mylast 'Man &myfirst; and &myfirst; mymiddle;.'>\r\n]>\r\n<!-- testing AttValue with references combination in single quote -->\r\n<student first='Full Name &myfirst; and &#x22;&mylast;&#x22; &mylast;' last='&mylast;'>My first Name is &myfirst; and my last name is &mylast;. </student>\r\n\r\n",
  224,
  "<student first=\"Full Name Snow and &quot;Man Snow and Snow mymiddle;.&quot; Man Snow and Snow mymiddle;.\" last=\"Man Snow and Snow mymiddle;.\">My first Name is Snow and my last name is Man Snow and Snow mymiddle;.. </student>"
},
{
  "ibm11v01.xml",
  242,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n   <!ELEMENT student (#PCDATA)>\r\n   <!ENTITY unref SYSTEM \"\">\r\n]>\r\n\r\n<!-- testing systemliteral with nothing between the double quotes -->\r\n<student>My Name is SnowMan. </student>\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n ",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm11v02.xml",
  225,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n   <!ELEMENT student (#PCDATA)>\r\n   <!ENTITY unref SYSTEM ''>\r\n]>\r\n\r\n<!-- testing systemliteral with nothing between the single quotes -->\r\n<student>My Name is SnowMan. </student>\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm11v03.xml",
  164,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student SYSTEM 'student.dtd'[\r\n]>\r\n<!-- testing systemliteral with a string with \"'\" -->\r\n<student>My Name is SnowMan. </student>\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm11v04.xml",
  169,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student SYSTEM \"student.dtd\" [\r\n]>\r\n\r\n<!-- testing systemliteral with a string with '\"' -->\r\n<student>My Name is SnowMan. </student>\r\n\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm12v01.xml",
  199,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student PUBLIC \"\" \"student.dtd\"[\r\n]>\r\n\r\n<!-- testing Pubid Literal with nothing between the double quote -->\r\n<student>My Name is SnowMan. </student>\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n ",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm12v02.xml",
  185,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student PUBLIC '' 'student.dtd'[\r\n]>\r\n\r\n<!-- testing Pubid Literal with nothing between the single quotes -->\r\n<student>My Name is SnowMan. </student>\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm12v03.xml",
  191,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student PUBLIC \"The big ' in it\" \"student.dtd\"[\r\n]>\r\n\r\n<!-- testing Pubid Literal with a string with \"'\" inside -->\r\n<student>My Name is SnowMan. </student>\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm12v04.xml",
  198,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student PUBLIC 'The latest version' 'student.dtd'[\r\n]>\r\n\r\n<!-- testing Pubid Literal with a string without  \"'\" inside -->\r\n<student>My Name is SnowMan. </student>\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm13v01.xml",
  283,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student PUBLIC \"#x20 #xD #xA abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ -'()+,./:=?;!*#@$_% \" \"student.dtd\"[\r\n]>\r\n\r\n<!-- testing Pubid char with all legal pubidchar in a string -->\r\n<student>My Name is SnowMan. </student>\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n ",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm14v01.xml",
  234,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student first CDATA #REQUIRED\r\n\t\t\t  last  CDATA #IMPLIED>\r\n]>\r\n\r\n<!-- testing chardata with empty string -->\r\n<student first=\"Snow\"></student>\r\n\r\n\r\n",
  32,
  "<student first=\"Snow\"></student>"
},
{
  "ibm14v02.xml",
  260,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student first CDATA #REQUIRED\r\n\t\t\t  last  CDATA #IMPLIED>\r\n]>\r\n\r\n<!-- testing chardata with white space -->\r\n<student first=\"Eric\"> &#x0A; &#x09; &#x0D;&#x20;</student>\r\n\r\n\r\n",
  50,
  "<student first=\"Eric\"> &#10; &#9; &#13; </student>"
},
{
  "ibm14v03.xml",
  324,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)>\r\n\t<!ATTLIST student first CDATA #REQUIRED\r\n\t\t\t  last  CDATA #IMPLIED>\r\n]>\r\n\r\n<!-- testing chardata with a string of sample legal char except '<' and '&' nor does it contain sequence \"]]>\" -->\r\n<student first=\"Snow\" last=\"Man\">This is a test</student>",
  57,
  "<student first=\"Snow\" last=\"Man\">This is a test</student>"
},
{
  "ibm15v01.xml",
  166,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n<!--* Tests empty comment *-->\r\n<!---->\r\n<student>My Name is SnowMan. </student>\r\n\r\n\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm15v02.xml",
  146,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<!-- Student's name -->\r\n<student>My Name is SnowMan. </student>",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm15v03.xml",
  146,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<!-- student file-1 -->\r\n<student>My Name is SnowMan. </student>",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm15v04.xml",
  172,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<!--student phone number 408-398 (387)-4758 -->\r\n<student>My Name is SnowMan. </student>\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm16v01.xml",
  143,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<?MyInstruct?>\r\n<student>My Name is SnowMan. </student>\r\n\r\n\r\n",
  54,
  "<?MyInstruct ?><student>My Name is SnowMan. </student>"
},
{
  "ibm16v02.xml",
  138,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<?MyInstruct ?>\r\n<student>My Name is SnowMan. </student>",
  54,
  "<?MyInstruct ?><student>My Name is SnowMan. </student>"
},
{
  "ibm16v03.xml",
  161,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<?MyInstruct AVOID ? BEFORE > IN PI ?>\r\n<student>My Name is SnowMan. </student>",
  77,
  "<?MyInstruct AVOID ? BEFORE > IN PI ?><student>My Name is SnowMan. </student>"
},
{
  "ibm17v01.xml",
  159,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<?MyInstruct This is a test ?>\r\n<student>My Name is SnowMan. </student>\r\n\r\n\r\n",
  69,
  "<?MyInstruct This is a test ?><student>My Name is SnowMan. </student>"
},
{
  "ibm18v01.xml",
  211,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<!-- testing CDSect with CDStart CData CDEnd -->\r\n\r\n<student>My Name is SnowMan. <![CDATA[This is <normal> text]]> </student>\r\n\r\n",
  67,
  "<student>My Name is SnowMan. This is &lt;normal&gt; text </student>"
},
{
  "ibm19v01.xml",
  180,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<!-- testing CDStart -->\r\n<student>My Name is SnowMan. <![CDATA[This is a test]]> </student>\r\n\r\n\r\n",
  54,
  "<student>My Name is SnowMan. This is a test </student>"
},
{
  "ibm20v01.xml",
  183,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<!-- testing CData with empty string -->\r\n\r\n<student>My Name is SnowMan. <![CDATA[]]></student>\r\n\r\n\r\n",
  39,
  "<student>My Name is SnowMan. </student>"
},
{
  "ibm20v02.xml",
  209,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<!-- testing CData with legal chars -->\r\n\r\n<student>My Name is SnowMan. <![CDATA[<testing>This is a test</testing>]]></student>",
  84,
  "<student>My Name is SnowMan. &lt;testing&gt;This is a test&lt;/testing&gt;</student>"
},
{
  "ibm21v01.xml",
  181,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE student [\r\n\t<!ELEMENT student (#PCDATA)> \r\n]>\r\n\r\n<!-- testing CDEnd --> \r\n\r\n<student>My Name is SnowMan. <![CDATA[This is a test]]> </student>\r\n\r\n\r\n",
  54,
  "<student>My Name is SnowMan. This is a test </student>"
},
{
  "ibm22v01.xml",
  92,
  "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm22v02.xml",
  51,
  "<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm22v03.xml",
  76,
  "<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<!-- This is a Misc -->\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm22v04.xml",
  76,
  "<!-- This is a Misc -->\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm22v05.xml",
  117,
  "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<!-- This is a Misc -->\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm22v06.xml",
  117,
  "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<!-- This is a Misc -->\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm22v07.xml",
  142,
  "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<!-- This is a Misc -->\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<!-- This is a Misc -->\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm23v01.xml",
  74,
  "<?xml version='1.0'?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm23v02.xml",
  92,
  "<?xml version='1.0' encoding='UTF-8' ?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm23v03.xml",
  92,
  "<?xml version='1.0' standalone='yes' ?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm23v04.xml",
  75,
  "<?xml version='1.0' ?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm23v05.xml",
  108,
  "<?xml version='1.0' encoding='UTF-8' standalone='yes'?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm23v06.xml",
  109,
  "<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm24v01.xml",
  74,
  "<?xml version='1.0'?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm24v02.xml",
  74,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm25v01.xml",
  74,
  "<?xml version='1.0'?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm25v02.xml",
  75,
  "<?xml version ='1.0'?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm25v03.xml",
  75,
  "<?xml version= '1.0'?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm25v04.xml",
  76,
  "<?xml version = '1.0'?>\r\n<!DOCTYPE doc [\r\n<!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm26v01.xml",
  78,
  "<?xml version='1.0' ?>\r\n<!DOCTYPE doc [\r\n   <!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n",
  11,
  "<doc></doc>"
},
{
  "ibm27v01.xml",
  112,
  "<?xml version='1.0' ?>\r\n<!DOCTYPE doc [\r\n   <!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n<!-- This is a comment in Misc -->",
  11,
  "<doc></doc>"
},
{
  "ibm27v02.xml",
  110,
  "<?xml version='1.0' ?>\r\n<!DOCTYPE doc [\r\n   <!ELEMENT doc EMPTY>\r\n]>\r\n<doc/>\r\n<?sound \"This is a PI in Misc ?>",
  43,
  "<doc></doc><?sound \"This is a PI in Misc ?>"
},
{
  "ibm27v03.xml",
  107,
  "<?xml version='1.0' ?>\r\n<!DOCTYPE doc [\r\n   <!ELEMENT doc ANY>\r\n]>\r\n<doc>S is in the following Misc</doc>\r\n",
  37,
  "<doc>S is in the following Misc</doc>"
},
{
  "ibm28v01.xml",
  157,
  "<?xml version=\"1.0\" encoding='UTF-8'?>\r\n<!DOCTYPE animal [\r\n   <!ELEMENT animal EMPTY>\r\n]>\r\n<!-- This a valid test file for production [28] --> \r\n<animal/>\r\n",
  17,
  "<animal></animal>"
},
{
  "ibm28v02.xml",
  805,
  "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<!DOCTYPE animal SYSTEM \"ibm28v02.dtd\" [\r\n   <!NOTATION animal_class SYSTEM \"ibm28v02.txt\">\r\n   <!ENTITY forcat \"This is a small cat\">\r\n   <!ELEMENT tiger (#PCDATA)>\r\n   <!ENTITY % make_small \"<!ELEMENT small EMPTY>\">\r\n   <!ENTITY % make_leopard_element \"<!ELEMENT leopard ANY>\">\r\n   <!ENTITY % make_attlist \"<!ATTLIST tiger color CDATA #REQUIRED>\">\r\n   %make_leopard_element; \r\n   <!ELEMENT cat ANY>\r\n   %make_small;\r\n   <!ENTITY % make_big \"<!ELEMENT big EMPTY>\">\r\n   %make_big;\r\n   %make_attlist;\r\n   <?sound \"This is a PI\" ?>\r\n   <!-- This is a valid test file for p28 -->\r\n]>\r\n<animal>\r\n   <cat>&forcat;</cat>\r\n   <tiger color=\"white\">This is a white tiger in Mirage!!</tiger>\r\n   <cat/>\r\n   <leopard>\r\n      <small/>\r\n      <big/>\r\n   </leopard>\r\n</animal>\r\n",
  326,
  "<?sound \"This is a PI\" ?><!DOCTYPE animal [\n<!NOTATION animal_class SYSTEM 'ibm28v02.txt'>\n]>\n<animal>&#10;   <cat>This is a small cat</cat>&#10;   <tiger color=\"white\">This is a white tiger in Mirage!!</tiger>&#10;   <cat></cat>&#10;   <leopard>&#10;      <small></small>&#10;      <big></big>&#10;   </leopard>&#10;</animal>"
},
{
  "ibm29v01.xml",
  627,
  "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<!DOCTYPE animal [\r\n   <!ELEMENT animal (cat|tiger|leopard)+>\r\n   <!NOTATION animal_class SYSTEM \"ibm29v01.txt\">\r\n   <!ELEMENT cat ANY>\r\n   <!ENTITY forcat \"This is a small cat\">\r\n   <!ELEMENT tiger (#PCDATA)>\r\n   <!ELEMENT leopard ANY>\r\n   <!ELEMENT small EMPTY>\r\n   <!ELEMENT big EMPTY>\r\n   <!ATTLIST tiger color CDATA #REQUIRED>\r\n   <?sound \"This is a PI\" ?>\r\n   <!-- This is a comment -->\r\n    \r\n]>\r\n<animal>\r\n   <cat>&forcat;</cat>\r\n   <tiger color=\"white\">This is a white tiger in Mirage!!</tiger>\r\n   <cat/>\r\n   <leopard>\r\n      <small/>\r\n      <big/>\r\n   </leopard>\r\n</animal>\r\n",
  326,
  "<?sound \"This is a PI\" ?><!DOCTYPE animal [\n<!NOTATION animal_class SYSTEM 'ibm29v01.txt'>\n]>\n<animal>&#10;   <cat>This is a small cat</cat>&#10;   <tiger color=\"white\">This is a white tiger in Mirage!!</tiger>&#10;   <cat></cat>&#10;   <leopard>&#10;      <small></small>&#10;      <big></big>&#10;   </leopard>&#10;</animal>"
},
{
  "ibm29v02.xml",
  690,
  "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<!DOCTYPE animal [\r\n   <!ELEMENT animal (cat|tiger|leopard)+>\r\n   <!NOTATION animal_class SYSTEM \"ibm29v01.txt\">\r\n   <!ELEMENT cat ANY>\r\n   <!ENTITY forcat \"This is a small cat\">\r\n   <!ELEMENT tiger (#PCDATA)>\r\n   <!ENTITY % make_leopard_element \"<!ELEMENT leopard ANY>\">\r\n   %make_leopard_element; \r\n   <!ELEMENT small EMPTY>\r\n   <!ELEMENT big EMPTY>\r\n   <!ATTLIST tiger color CDATA #REQUIRED>\r\n   <?sound \"This is a PI\" ?>\r\n   <!-- This is a comment -->\r\n    \r\n]>\r\n<animal>\r\n   <cat>&forcat;</cat>\r\n   <tiger color=\"white\">This is a white tiger in Mirage!!</tiger>\r\n   <cat/>\r\n   <leopard>\r\n      <small/>\r\n      <big/>\r\n   </leopard>\r\n</animal>\r\n",
  326,
  "<?sound \"This is a PI\" ?><!DOCTYPE animal [\n<!NOTATION animal_class SYSTEM 'ibm29v01.txt'>\n]>\n<animal>&#10;   <cat>This is a small cat</cat>&#10;   <tiger color=\"white\">This is a white tiger in Mirage!!</tiger>&#10;   <cat></cat>&#10;   <leopard>&#10;      <small></small>&#10;      <big></big>&#10;   </leopard>&#10;</animal>"
},
{
  "ibm30v01.xml",
  118,
  "<!DOCTYPE animal SYSTEM \"ibm30v01.dtd\">\r\n<animal/>\r\n<!-- tests extSubset with extSubsetDecl only in the dtd file -->\r\n",
  17,
  "<animal></animal>"
},
{
  "ibm30v02.xml",
  126,
  "<!DOCTYPE animal SYSTEM \"ibm30v02.dtd\">\r\n<animal/>\r\n<!-- tests extSubset with TextDecl and extSubsetDecl in the dtd file -->\r\n",
  17,
  "<animal></animal>"
},
{
  "ibm31v01.xml",
  187,
  "<!DOCTYPE animal SYSTEM \"ibm31v01.dtd\">\r\n<animal>\r\n   <tiger/>\r\n</animal>\r\n<!-- tests extSubsetDecl with combinations of markupdecls, conditionalSects, PEReferences and white spaces -->\r\n",
  45,
  "<animal>&#10;   <tiger></tiger>&#10;</animal>"
},
{
  "ibm32v01.xml",
  210,
  "<?xml version=\"1.0\" standalone=\"no\" ?>\r\n<!DOCTYPE animal SYSTEM \"ibm32v01.dtd\">\r\n<animal/>\r\n<!-- Tests VC: Standalone Document Declaration with absent attribute that has default value and standalone is no -->\r\n",
  32,
  "<animal color=\"yellow\"></animal>"
},
{
  "ibm32v02.xml",
  221,
  "<?xml version=\"1.0\" standalone=\"no\" ?>\r\n<!DOCTYPE animal SYSTEM \"ibm32v02.dtd\">\r\n<animal>&animal_content;</animal>\r\n<!-- Tests VC: Standalone Document Declaration with external enitity reference and standalone is no -->\r\n",
  39,
  "<animal>This is a yellow tiger</animal>"
},
{
  "ibm32v03.xml",
  214,
  "<?xml version=\"1.0\" standalone=\"no\" ?>\r\n<!DOCTYPE animal SYSTEM \"ibm32v03.dtd\">\r\n<animal/>\r\n<!-- Tests VC: Standalone Document Declaration with attribute values that need to be normalized and standalone is no -->\r\n",
  27,
  "<animal class=\")\"></animal>"
},
{
  "ibm32v04.xml",
  225,
  "<?xml version=\"1.0\" standalone=\"no\" ?>\r\n<!DOCTYPE animal SYSTEM \"ibm32v04.dtd\">\r\n<animal>This is a \r\n      <a/>  \r\n\r\nyellow tiger</animal>\r\n<!-- Tests VC: Standalone Document Declaration with whitespace in mixed content -->\r\n",
  90,
  "<animal xml:space=\"preserve\">This is a &#10;      <a></a>  &#10;&#10;yellow tiger</animal>"
},
{
  "ibm33v01.xml",
  196,
  "<!DOCTYPE book [\r\n   <!ELEMENT book ANY>\r\n   <!ATTLIST book xml:lang CDATA #REQUIRED>\r\n]>\r\n<book xml:lang=\"en-US\">It is written in English</book>\r\n<!-- Tests LanguageID with Langcode - Subcode -->",
  54,
  "<book xml:lang=\"en-US\">It is written in English</book>"
},
{
  "ibm34v01.xml",
  147,
  "<!DOCTYPE book [\r\n   <!ELEMENT book ANY>\r\n   <!ATTLIST book xml:lang CDATA #REQUIRED>\r\n]>\r\n<book xml:lang=\"en-US\">It is written in English</book>\r\n",
  54,
  "<book xml:lang=\"en-US\">It is written in English</book>"
},
{
  "ibm35v01.xml",
  144,
  "<!DOCTYPE book [\r\n   <!ELEMENT book ANY>\r\n   <!ATTLIST book xml:lang CDATA #REQUIRED>\r\n]>\r\n<book xml:lang=\"en\">It is written in English</book>\r\n",
  51,
  "<book xml:lang=\"en\">It is written in English</book>"
},
{
  "ibm36v01.xml",
  151,
  "<!DOCTYPE book [\r\n   <!ELEMENT book ANY>\r\n   <!ATTLIST book xml:lang CDATA #REQUIRED>\r\n]>\r\n<book xml:lang=\"i-BS-ABCD\">It is written in English</book>\r\n",
  58,
  "<book xml:lang=\"i-BS-ABCD\">It is written in English</book>"
},
{
  "ibm37v01.xml",
  150,
  "<!DOCTYPE book [\r\n   <!ELEMENT book ANY>\r\n   <!ATTLIST book xml:lang CDATA #REQUIRED>\r\n]>\r\n<book xml:lang=\"x-uk-eng\">It is written in English</book>\r\n",
  57,
  "<book xml:lang=\"x-uk-eng\">It is written in English</book>"
},
{
  "ibm38v01.xml",
  148,
  "<!DOCTYPE book [\r\n   <!ELEMENT book ANY>\r\n   <!ATTLIST book xml:lang CDATA #REQUIRED>\r\n]>\r\n<book xml:lang=\"en-USa\">It is written in English</book>\r\n",
  55,
  "<book xml:lang=\"en-USa\">It is written in English</book>"
},
{
  "ibm39v01.xml",
  399,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (a,b)>\r\n  <!ELEMENT a EMPTY>\r\n  <!ELEMENT b (#PCDATA|c)* >\r\n  <!ELEMENT c ANY>\r\n  <!ELEMENT d ((e,e)|f)+ >\r\n  <!ELEMENT e ANY>\r\n  <!ELEMENT f EMPTY>\r\n]>\r\n<root><a/><b>\r\n   <c></c> \r\n   content of b element\r\n   <c>\r\n      <d><e>no more children</e><e><f/></e><f/></d>\r\n   </c>\r\n</b></root>\r\n<!--* test P39's syntax and Element Valid VC *-->\r\n",
  161,
  "<root><a></a><b>&#10;   <c></c> &#10;   content of b element&#10;   <c>&#10;      <d><e>no more children</e><e><f></f></e><f></f></d>&#10;   </c>&#10;</b></root>"
},
{
  "ibm40v01.xml",
  428,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA|b)* >\r\n  <!ELEMENT b (#PCDATA) >\r\n  <!ATTLIST b attr1 CDATA #IMPLIED>\r\n  <!ATTLIST b attr2 CDATA #IMPLIED>\r\n  <!ATTLIST b attr3 CDATA #IMPLIED>\r\n]>\r\n<root>\r\n  <b>without white space</b>\r\n  <b > with a white space</b>\r\n  <b attr1=\"value1\">one attribute</b>\r\n  <b attr1=\"value1\" attr2=\"value2\" attr3 = \"value3\">one attribute</b>\r\n</root>\r\n<!--* testing P40 *-->\r\n",
  198,
  "<root>&#10;  <b>without white space</b>&#10;  <b> with a white space</b>&#10;  <b attr1=\"value1\">one attribute</b>&#10;  <b attr1=\"value1\" attr2=\"value2\" attr3=\"value3\">one attribute</b>&#10;</root>"
},
{
  "ibm41v01.xml",
  333,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA|b)* >\r\n  <!ELEMENT b (#PCDATA) >\r\n  <!ATTLIST b attr1 CDATA #REQUIRED>\r\n  <!ATTLIST b attr2 (abc|def) \"abc\">\r\n  <!ATTLIST b attr3 CDATA #FIXED \"fixed\">\r\n]>\r\n<root>\r\n  <b attr1=\"value1\" attr2=\"def\" attr3=\"fixed\">Name eq AttValue</b>\r\n</root>\r\n<!--* testing P41 *-->\r\n",
  89,
  "<root>&#10;  <b attr1=\"value1\" attr2=\"def\" attr3=\"fixed\">Name eq AttValue</b>&#10;</root>"
},
{
  "ibm42v01.xml",
  261,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (a,b)>\r\n  <!ELEMENT a EMPTY>\r\n  <!ELEMENT b (#PCDATA|c)* >\r\n  <!ELEMENT c ANY>\r\n]>\r\n<root><a/><b>\r\n   <c></c > : End tag with a space inside\r\n   content of b element\r\n</b></root>\r\n<!--* test P42 *-->\r\n\r\n",
  105,
  "<root><a></a><b>&#10;   <c></c> : End tag with a space inside&#10;   content of b element&#10;</b></root>"
},
{
  "ibm43v01.xml",
  746,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (a,b)>\r\n  <!ELEMENT a EMPTY>\r\n  <!ELEMENT b (#PCDATA|c)* >\r\n  <!ELEMENT c ANY>\r\n  <!ENTITY inContent \"<b>General entity reference in element content</b>\">\r\n]>\r\n<!--* content: element|CharData|Reference|CDSect|PI|CDSect|PI|Comment *-->\r\n<root><a/><b>\r\n<!-- there is an empty element in the above line -->\r\n   <c></c> \r\n   CharData: content of b element\r\n   %paaa; : PE reference should not be recognized in element content \r\n   <c>\r\n<?PIcontent anyProcessingInstruction?>\r\n<!-- Comment content -->\r\n    &inContent;\r\n    Charater reference: &#x41;\r\n    CDSect in content: <![CDATA[ <html>markups<head>HEAD</head><body>nothing</body></html> ]]>\r\n   </c>\r\n</b>\r\n</root>\r\n<!--* test P43 *-->\r\n",
  453,
  "<root><a></a><b>&#10;&#10;   <c></c> &#10;   CharData: content of b element&#10;   %paaa; : PE reference should not be recognized in element content &#10;   <c>&#10;<?PIcontent anyProcessingInstruction?>&#10;&#10;    <b>General entity reference in element content</b>&#10;    Charater reference: A&#10;    CDSect in content:  &lt;html&gt;markups&lt;head&gt;HEAD&lt;/head&gt;&lt;body&gt;nothing&lt;/body&gt;&lt;/html&gt; &#10;   </c>&#10;</b>&#10;</root>"
},
{
  "ibm44v01.xml",
  393,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA|b)* >\r\n  <!ELEMENT b EMPTY >\r\n  <!ATTLIST b attr1 CDATA #IMPLIED>\r\n  <!ATTLIST b attr2 CDATA #IMPLIED>\r\n  <!ATTLIST b attr3 CDATA #IMPLIED>\r\n]>\r\n<root>\r\n  <b/>without white space\r\n  <b /> with a white space\r\n  <b attr1=\"value1\" />\r\n  <b attr1=\"value1\" attr2=\"value2\" attr3 = \"value3\"/>\r\n</root>\r\n<!--* testing P44 *-->\r\n\r\n\r\n\r\n",
  172,
  "<root>&#10;  <b></b>without white space&#10;  <b></b> with a white space&#10;  <b attr1=\"value1\"></b>&#10;  <b attr1=\"value1\" attr2=\"value2\" attr3=\"value3\"></b>&#10;</root>"
},
{
  "ibm45v01.xml",
  558,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA|b)* >\r\n  <!--* P45 no space before the end bracket *-->\r\n  <!ELEMENT b EMPTY>\r\n  <!ELEMENT unique ANY>\r\n  <!ELEMENT unique- ANY>\r\n  <!ELEMENT unique_ ANY>\r\n  <!ELEMENT unique. ANY>\r\n  <!ATTLIST b attr1 CDATA #IMPLIED>\r\n  <!ATTLIST b attr2 CDATA #IMPLIED>\r\n  <!ATTLIST b attr3 CDATA #IMPLIED>\r\n]>\r\n<root>\r\n  <b/>without white space\r\n  <b /> with a white space\r\n  <b attr1=\"value1\" />\r\n  <b attr1=\"value1\" attr2=\"value2\" attr3 = \"value3\"/>\r\n</root>\r\n<!--* !!! testing both P45 and p46 *-->\r\n\r\n",
  172,
  "<root>&#10;  <b></b>without white space&#10;  <b></b> with a white space&#10;  <b attr1=\"value1\"></b>&#10;  <b attr1=\"value1\" attr2=\"value2\" attr3=\"value3\"></b>&#10;</root>"
},
{
  "ibm47v01.xml",
  827,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (a,b)>\r\n  <!ELEMENT a EMPTY>\r\n  <!ELEMENT b (#PCDATA|c)* >\r\n  <!ELEMENT c ANY>\r\n  <!ELEMENT d ANY>\r\n  <!ELEMENT e ANY>\r\n  <!ELEMENT f ANY>\r\n  <!--* test all possible children,cp,choice,seq patterns in P47,P48,P49,P50 *-->\r\n  <!ELEMENT child0 (a)>\r\n  <!ELEMENT child1 (a|b|c)>\r\n  <!ELEMENT child2 (a ,b,b?,a*,c,c,a,a,b+,c ) >\r\n  <!ELEMENT child3 (a+|b)? >\r\n  <!ELEMENT child4 (a, (b|c)+, (a|d)?, (e|f)* )?>\r\n  <!ELEMENT child5 ( (a,b) | c? | ((d|e),b,c) )* >\r\n  <!ELEMENT child5_1 ( (a,b)* | (c,b)? | (d,a)+ | ((e|f),b,c) )* >\r\n  <!ELEMENT child6 (a,b,c)*>\r\n  <!ELEMENT child7 ((a,b)|c*|((d|e),b,c) )+ >\r\n  <!ELEMENT child8 ( a, (b|c), (a|b), b)+>  \r\n]>\r\n<root><a/><b>\r\n   <c></c >\r\n   content of b element\r\n</b></root>\r\n<!--* a valid test: tests P47,P48,P49,P50*-->\r\n\r\n",
  75,
  "<root><a></a><b>&#10;   <c></c>&#10;   content of b element&#10;</b></root>"
},
{
  "ibm49v01.xml",
  228,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm49v01.dtd\"[\r\n  <!ELEMENT root (a,b)>\r\n]>\r\n<root><a/><b>\r\n   <c></c>\r\n   content of b element\r\n</b></root>\r\n<!--* a valid test: tests VC:Proper Group/PE Nesting in P49 *-->\r\n\r\n\r\n\r\n",
  75,
  "<root><a></a><b>&#10;   <c></c>&#10;   content of b element&#10;</b></root>"
},
{
  "ibm50v01.xml",
  307,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm50v01.dtd\" [\r\n  <!ELEMENT root (a,b)>\r\n]>\r\n<root><a/><b>\r\n   <c><child1><a/><b></b><c></c></child1></c >\r\n   <c><child2><a/><b></b><c></c></child2></c >\r\n   content of b element\r\n</b></root>\r\n<!--* a valid test: tests VC:Proper Group/PE Nesting in P50 *-->\r\n",
  166,
  "<root><a></a><b>&#10;   <c><child1><a></a><b></b><c></c></child1></c>&#10;   <c><child2><a></a><b></b><c></c></child2></c>&#10;   content of b element&#10;</b></root>"
},
{
  "ibm51v01.xml",
  604,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root ANY>\r\n  <!--* test all possible Mixed content type decl *-->\r\n  <!ELEMENT a (#PCDATA)* >\r\n  <!ELEMENT b (#PCDATA) >\r\n  <!ELEMENT c ( #PCDATA)*>\r\n  <!ELEMENT d (#PCDATA|c)*>\r\n  <!ELEMENT e (#PCDATA|c| b|a)* >\r\n  <!ELEMENT f (#PCDATA| c)* >\r\n  <!ELEMENT g ( #PCDATA) >\r\n  <!ELEMENT h (#PCDATA )>\r\n  <!ELEMENT i ( #PCDATA ) >\r\n]>\r\n<root>\r\n  <a> Element type a </a>\r\n  <b> Element type b </b>\r\n  <c> Element type c </c>\r\n  <d> Element type d <c></c> </d>\r\n  <e> Element type e <a></a> <b></b> <c></c> </e>\r\n</root>\r\n<!--* a valid test: tests P51 *-->",
  200,
  "<root>&#10;  <a> Element type a </a>&#10;  <b> Element type b </b>&#10;  <c> Element type c </c>&#10;  <d> Element type d <c></c> </d>&#10;  <e> Element type e <a></a> <b></b> <c></c> </e>&#10;</root>"
},
{
  "ibm51v02.xml",
  336,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm51v02.dtd\" [\r\n  <!ELEMENT root ANY>\r\n]>\r\n<root>\r\n  <a> Element type a </a>\r\n  <b> Element type b </b>\r\n  <c> Element type c </c>\r\n  <d> Element type d <c></c> </d>\r\n  <e> Element type e <a></a> <b></b> <c></c> </e>\r\n</root>\r\n<!--* a valid test: tests P51 VC: Proper Group/PE Nesting *-->",
  200,
  "<root>&#10;  <a> Element type a </a>&#10;  <b> Element type b </b>&#10;  <c> Element type c </c>&#10;  <d> Element type d <c></c> </d>&#10;  <e> Element type e <a></a> <b></b> <c></c> </e>&#10;</root>"
},
{
  "ibm52v01.xml",
  532,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root ANY>\r\n  <!ELEMENT a (#PCDATA)* >\r\n  <!ELEMENT b (#PCDATA) >\r\n  <!--* All AttlistDecl Patterns in P52 and P53 *-->\r\n  <!ATTLIST a>\r\n  <!ATTLIST a >\r\n  <!ATTLIST b battr1 CDATA #REQUIRED >\r\n  <!ATTLIST b battr2 CDATA #IMPLIED \r\n              battr3 CDATA #FIXED \"fixedvalue\" battr4 (abc|def) \"abc\" >\r\n]>\r\n<root>\r\n  <a> Element type a </a>\r\n  <b battr1 = \"anyvalue\" battr3=\"fixedvalue\" battr4 =\"def\"> test P52 and P53 </b>\r\n</root>\r\n<!--* a valid test: tests P52 and P53 *-->\r\n",
  131,
  "<root>&#10;  <a> Element type a </a>&#10;  <b battr1=\"anyvalue\" battr3=\"fixedvalue\" battr4=\"def\"> test P52 and P53 </b>&#10;</root>"
},
{
  "ibm54v01.xml",
  1857,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root ANY>\r\n  <!ELEMENT a (#PCDATA) >\r\n  <!ELEMENT b (#PCDATA) >\r\n  <!ELEMENT c (#PCDATA) >\r\n  <!ELEMENT d (#PCDATA) >\r\n  <!ELEMENT e (#PCDATA) >\r\n  <!ELEMENT f (#PCDATA) >\r\n  <!ELEMENT g (#PCDATA) >\r\n  <!ELEMENT h (#PCDATA) >\r\n  <!ELEMENT i (#PCDATA) >\r\n  <!ELEMENT j (#PCDATA) >\r\n  <!ELEMENT k (#PCDATA) >\r\n  <!--* Tests all Attbuite types in P55,P56,P57,P58,P59 *-->\r\n  <!ATTLIST a aattr1 ID #REQUIRED>\r\n  <!ATTLIST b battr1 CDATA #REQUIRED \r\n              battr2 (good|bad) #REQUIRED \r\n              battr3 ID #REQUIRED>\r\n  <!ATTLIST c c_reference IDREF #REQUIRED>\r\n  <!ATTLIST d d_reference IDREFS #REQUIRED>\r\n  <!ENTITY xmltech SYSTEM \"xmltech.gif\" NDATA gif>\r\n  <!NOTATION gif SYSTEM \"gif\">\r\n  <!ATTLIST e eattr1 ENTITY #REQUIRED>\r\n  <!ENTITY IBMlogo SYSTEM \"IBMlogo.gif\" NDATA gif>\r\n  <!ATTLIST f fattr1 ENTITIES #REQUIRED>\r\n  <!ATTLIST g gattr1 NMTOKEN #REQUIRED>\r\n  <!ATTLIST h hattr1 NMTOKENS #REQUIRED>\r\n  <!NOTATION UTF-8 SYSTEM \"UTF-8\">\r\n  <!ATTLIST i iattr1 NOTATION (UTF-8) #REQUIRED>\r\n  <!--* Tests all DefaultDecl in P60 -->\r\n  <!ATTLIST j jattr1 CDATA #REQUIRED \r\n              jattr2 CDATA \"good\" \r\n              jattr3 CDATA #FIXED \"fixed\"\r\n\t      jattr4 CDATA #IMPLIED >\r\n\r\n]>\r\n<root>\r\n  <a aattr1 = \"a1\"> Element type a </a>\r\n  <b battr1 = \"anyvalue\" battr2=\"good\" battr3 =\"b1\"> Element type b </b>\r\n  <c c_reference = \"b1\"> Element type c </c>\r\n  <d d_reference = \"a1 b1\"> Element type d </d>\r\n  <e eattr1 = \"xmltech\"> Element type e </e>\r\n  <f fattr1 = \"xmltech IBMlogo\"> Element type f </f>\r\n  <g gattr1 = \"xml4j3_0_0_EA3\"> Element type g </g>\r\n  <h hattr1 = \"xml4j3_0_0_EA3 Xerces-J_1_0_1\"> Element type h </h>\r\n  <i iattr1 = \"UTF-8\"> Element type i </i>\r\n  <j jattr1 = \"anyvalue\" jattr2=\"good\" jattr3 =\"fixed\"> Element type j </j>\r\n</root>\r\n<!--* a valid test: tests P54 *-->\r\n",
  660,
  "<!DOCTYPE root [\n<!NOTATION UTF-8 SYSTEM 'UTF-8'>\n<!NOTATION gif SYSTEM 'gif'>\n]>\n<root>&#10;  <a aattr1=\"a1\"> Element type a </a>&#10;  <b battr1=\"anyvalue\" battr2=\"good\" battr3=\"b1\"> Element type b </b>&#10;  <c c_reference=\"b1\"> Element type c </c>&#10;  <d d_reference=\"a1 b1\"> Element type d </d>&#10;  <e eattr1=\"xmltech\"> Element type e </e>&#10;  <f fattr1=\"xmltech IBMlogo\"> Element type f </f>&#10;  <g gattr1=\"xml4j3_0_0_EA3\"> Element type g </g>&#10;  <h hattr1=\"xml4j3_0_0_EA3 Xerces-J_1_0_1\"> Element type h </h>&#10;  <i iattr1=\"UTF-8\"> Element type i </i>&#10;  <j jattr1=\"anyvalue\" jattr2=\"good\" jattr3=\"fixed\"> Element type j </j>&#10;</root>"
},
{
  "ibm54v02.xml",
  368,
  "<?xml  version=\"1.0\"?>\r\n<!-- test for Production 54-->\r\n<!DOCTYPE root\r\n [\r\n <!ELEMENT root (x|y|z)*>\r\n <!ELEMENT x (#PCDATA)>\r\n <!ELEMENT y ANY>\r\n <!ELEMENT z EMPTY>\r\n <!ATTLIST x attr CDATA #IMPLIED>\r\n <!ATTLIST y attr NMTOKENS #IMPLIED>\r\n <!ATTLIST z attr (x|y) #IMPLIED>\r\n ]>\r\n<root>\r\n<x attr= 'Madhu'></x>\r\n<y attr= '1.a.name.token.but.not.a.name'></y>\r\n</root>\r\n",
  92,
  "<root>&#10;<x attr=\"Madhu\"></x>&#10;<y attr=\"1.a.name.token.but.not.a.name\"></y>&#10;</root>"
},
{
  "ibm54v03.xml",
  220,
  "<?xml  version=\"1.0\"?>\r\n<!-- test for Production 54-->\r\n<!DOCTYPE AttrType\r\n[\r\n<!ELEMENT AttrType ANY>\r\n<!ELEMENT a (#PCDATA)>\r\n<!ATTLIST a att CDATA #IMPLIED>\r\n]>\r\n<AttrType>\r\n<a att= 'hello world'>\r\n</a>\r\n</AttrType>\r\n",
  61,
  "<AttrType>&#10;<a att=\"hello world\">&#10;</a>&#10;</AttrType>"
},
{
  "ibm55v01.xml",
  254,
  "<?xml  version=\"1.0\"?>\r\n<!-- test valid syntax for Production 55-->\r\n<!DOCTYPE StType\r\n[\r\n<!ELEMENT StType ANY>\r\n<!ELEMENT a EMPTY>\r\n<!ATTLIST a att CDATA #IMPLIED> \r\n]>\r\n<StType>\r\n<a att='Hello'/>\r\nTesting with a valid stringType attribute \r\n</StType>\r\n",
  93,
  "<StType>&#10;<a att=\"Hello\"></a>&#10;Testing with a valid stringType attribute &#10;</StType>"
},
{
  "ibm56v01.xml",
  530,
  "<?xml  version=\"1.0\"?>\r\n<!-- valid test for Production 56-->\r\n<!DOCTYPE root\r\n [\r\n <!ELEMENT root (a|b|c|d|e|f|g)*>\r\n <!ELEMENT a EMPTY>\r\n <!ELEMENT b EMPTY>\r\n <!ELEMENT c EMPTY>\r\n <!ELEMENT d EMPTY>\r\n <!ELEMENT e EMPTY>\r\n <!ELEMENT f EMPTY>\r\n <!ELEMENT g EMPTY>\r\n <!ATTLIST a attr ID #IMPLIED>\r\n <!ATTLIST b attr IDREF #IMPLIED>\r\n <!ATTLIST c attr IDREFS #IMPLIED>\r\n <!ATTLIST d attr ENTITY #IMPLIED>\r\n <!ATTLIST e attr ENTITIES #IMPLIED>\r\n <!ATTLIST f attr NMTOKEN #IMPLIED>\r\n <!ATTLIST g attr NMTOKENS #IMPLIED>\r\n ]>\r\n<root/>\r\n",
  13,
  "<root></root>"
},
{
  "ibm56v02.xml",
  374,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(Positive Test)-->\r\n<!DOCTYPE tokenizer\r\n [\r\n <!ELEMENT tokenizer ANY>\r\n <!ATTLIST tokenizer UniqueName ID #REQUIRED>\r\n ]>\r\n<tokenizer UniqueName = \"AC1999\">\r\nThis is a positive test for validity constraints\r\nGiving a unique name to the attribute ID an ID Attribute default as #required\r\n</tokenizer>\r\n",
  183,
  "<tokenizer UniqueName=\"AC1999\">&#10;This is a positive test for validity constraints&#10;Giving a unique name to the attribute ID an ID Attribute default as #required&#10;</tokenizer>"
},
{
  "ibm56v03.xml",
  333,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(Positive Test)-->\r\n<!DOCTYPE tokenizer\r\n [\r\n <!ELEMENT tokenizer ANY>\r\n <!ATTLIST tokenizer UniqueName ID #IMPLIED>\r\n ]>\r\n<tokenizer UniqueName = \"AC1999\">\r\nThis is a positive test for validity constraints\r\nGiving ID attribute default as #IMPLIED\r\n</tokenizer>",
  145,
  "<tokenizer UniqueName=\"AC1999\">&#10;This is a positive test for validity constraints&#10;Giving ID attribute default as #IMPLIED&#10;</tokenizer>"
},
{
  "ibm56v04.xml",
  467,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(positive Test)-->\r\n<!DOCTYPE tokenizer\r\n [\r\n <!ELEMENT tokenizer ANY>\r\n <!ELEMENT b EMPTY>\r\n <!ATTLIST b attr ID #REQUIRED>\r\n <!ATTLIST tokenizer UniqueName ID #REQUIRED>\r\n  ]>\r\n<tokenizer UniqueName = \"Ac999\">\r\n<b attr = \"BC999\"></b>\r\nThis is a positive test for validity constraints\r\nthe value of the attribute with a type ID does not appear more than once in the XML document\r\n</tokenizer>\r\n",
  222,
  "<tokenizer UniqueName=\"Ac999\">&#10;<b attr=\"BC999\"></b>&#10;This is a positive test for validity constraints&#10;the value of the attribute with a type ID does not appear more than once in the XML document&#10;</tokenizer>"
},
{
  "ibm56v05.xml",
  449,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(positive Test)-->\r\n<!DOCTYPE tokenizer\r\n [\r\n <!ELEMENT tokenizer ANY>\r\n <!ELEMENT a EMPTY>\r\n <!ELEMENT b EMPTY>\r\n <!ATTLIST a first ID #REQUIRED>\r\n <!ATTLIST b second ID #REQUIRED>\r\n ]>\r\n<tokenizer>\r\n<a first = \"AC1999\"></a>\r\n<b second = \"CD345\"></b>\r\nThis is a positive validity test for ID.\r\nany element type has no more than one attribute of type ID specified\r\n</tokenizer>",
  200,
  "<tokenizer>&#10;<a first=\"AC1999\"></a>&#10;<b second=\"CD345\"></b>&#10;This is a positive validity test for ID.&#10;any element type has no more than one attribute of type ID specified&#10;</tokenizer>"
},
{
  "ibm56v06.xml",
  572,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(Positive Test)-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT id EMPTY>\r\n <!ELEMENT idref EMPTY>\r\n <!ATTLIST id UniqueName ID #REQUIRED>\r\n <!ATTLIST idref reference IDREF #IMPLIED>\r\n ]>\r\n<test>\r\n<id UniqueName = \"AC456\"></id>\r\n<idref reference = \"AC456\"></idref>\r\nPositive test for validity constraint of IDREF.\r\nIn an attribute decl, values of type IDREF match tha name production\r\nand the IDREF value matches the value assigned to an ID attribute somewhere\r\nin the XML document.\r\n</test>",
  319,
  "<test>&#10;<id UniqueName=\"AC456\"></id>&#10;<idref reference=\"AC456\"></idref>&#10;Positive test for validity constraint of IDREF.&#10;In an attribute decl, values of type IDREF match tha name production&#10;and the IDREF value matches the value assigned to an ID attribute somewhere&#10;in the XML document.&#10;</test>"
},
{
  "ibm56v07.xml",
  673,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(Positive Test)-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT id1 EMPTY>\r\n <!ELEMENT id2 EMPTY>\r\n <!ELEMENT idref EMPTY>\r\n <!ATTLIST id1 UniqueName ID #REQUIRED>\r\n <!ATTLIST id2 UName ID #IMPLIED>\r\n <!ATTLIST idref reference IDREFS #IMPLIED>\r\n ]>\r\n<test>\r\n<id1 UniqueName = \"AC456\"></id1>\r\n<id2 UName = \"Q123\"></id2>\r\n<idref reference = \"AC456 Q123\"></idref>\r\nPositive test for validity constraint of IDREFS.\r\nIn an attribute decl, values of type IDREFS match tha name production\r\nand the IDREFS value matches the values assigned to an ID attributes somewhere\r\nin the XML document.\r\n</test>",
  360,
  "<test>&#10;<id1 UniqueName=\"AC456\"></id1>&#10;<id2 UName=\"Q123\"></id2>&#10;<idref reference=\"AC456 Q123\"></idref>&#10;Positive test for validity constraint of IDREFS.&#10;In an attribute decl, values of type IDREFS match tha name production&#10;and the IDREFS value matches the values assigned to an ID attributes somewhere&#10;in the XML document.&#10;</test>"
},
{
  "ibm56v08.xml",
  503,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(Positive Test)it is a DTD-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT landscape EMPTY>\r\n <!NOTATION gif PUBLIC \"gif\">\r\n <!ENTITY image SYSTEM \"testspec/images/sunset.gif\" NDATA gif>\r\n <!ATTLIST landscape sun ENTITY #IMPLIED>\r\n]>\r\n<test>\r\n<landscape sun = \"image\"></landscape>\r\nvalues of type ENTITY match the Name production and the ENTITY value\r\nmatches the name of an unparsed entity declared in the DTD.\r\n</test>\r\n",
  244,
  "<!DOCTYPE test [\n<!NOTATION gif PUBLIC 'gif'>\n]>\n<test>&#10;<landscape sun=\"image\"></landscape>&#10;values of type ENTITY match the Name production and the ENTITY value&#10;matches the name of an unparsed entity declared in the DTD.&#10;</test>"
},
{
  "ibm56v09.xml",
  361,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(Positive Test)-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT nametoken EMPTY>\r\n <!ATTLIST nametoken thistoken NMTOKEN #IMPLIED>\r\n]>\r\n<test>\r\n<nametoken thistoken = \"x:image\"></nametoken>\r\nIn an attribute declaration, values of type NMTOKEN match the Nmtoken production\r\n</test>",
  151,
  "<test>&#10;<nametoken thistoken=\"x:image\"></nametoken>&#10;In an attribute declaration, values of type NMTOKEN match the Nmtoken production&#10;</test>"
},
{
  "ibm56v10.xml",
  377,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(Positive Test)-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT nametokens EMPTY>\r\n <!ATTLIST nametokens thistoken NMTOKENS #IMPLIED>\r\n]>\r\n<test>\r\n<nametokens thistoken = \"x:lang y:country\"></nametokens>\r\nIn an attribute declaration, values of type NMTOKENS match the Nmtokens production\r\n</test>",
  164,
  "<test>&#10;<nametokens thistoken=\"x:lang y:country\"></nametokens>&#10;In an attribute declaration, values of type NMTOKENS match the Nmtokens production&#10;</test>"
},
{
  "ibm57v01.xml",
  370,
  "<?xml  version=\"1.0\"?>\r\n<!-- valid test for Production 57-->\r\n<!DOCTYPE root\r\n [\r\n <!ELEMENT root (#PCDATA|a|b)*>\r\n <!ELEMENT a ANY>\r\n <!ELEMENT b ANY>\r\n <!NOTATION a SYSTEM \"a\">\r\n <!NOTATION b SYSTEM \"b\"> \r\n <!ATTLIST a att (a|b) #IMPLIED>\r\n <!ATTLIST b att NOTATION (a|b) #IMPLIED>\r\n ]>\r\n <root>\r\nThis test case tests the kinds of enumerated types\r\n<a/><b/>\r\n</root>\r\n",
  162,
  "<!DOCTYPE root [\n<!NOTATION a SYSTEM 'a'>\n<!NOTATION b SYSTEM 'b'>\n]>\n<root>&#10;This test case tests the kinds of enumerated types&#10;<a></a><b></b>&#10;</root>"
},
{
  "ibm58v01.xml",
  618,
  "<?xml  version=\"1.0\"?>\r\n<!-- valid test for Production 58-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT one ANY>\r\n <!ELEMENT two ANY>\r\n <!ELEMENT three ANY>\r\n <!ELEMENT four ANY>\r\n <!ELEMENT five ANY>\r\n <!NOTATION this SYSTEM \"alpha\">\r\n <!NOTATION that SYSTEM \"beta\">\r\n <!ATTLIST one attr NOTATION (this) #IMPLIED>\r\n <!ATTLIST two attr NOTATION ( this) #IMPLIED>\r\n <!ATTLIST three attr NOTATION (this|that) #IMPLIED>\r\n <!ATTLIST four attr NOTATION (that |this) #IMPLIED>\r\n <!ATTLIST five attr NOTATION ( that ) #IMPLIED>\r\n ]>\r\n <test>\r\nThis is a positive test with different patterns for NOTATION\r\n</test>\r\n",
  166,
  "<!DOCTYPE test [\n<!NOTATION that SYSTEM 'beta'>\n<!NOTATION this SYSTEM 'alpha'>\n]>\n<test>&#10;This is a positive test with different patterns for NOTATION&#10;</test>"
},
{
  "ibm58v02.xml",
  548,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 58-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT blob (#PCDATA)>\r\n <!NOTATION base64 SYSTEM \"mimecode\">\r\n <!NOTATION uuencode SYSTEM \"uudecode\">\r\n <!NOTATION raw SYSTEM \"bin/cat\">\r\n <!ATTLIST blob content-encoding NOTATION (base64|uuencode|raw) #REQUIRED>\r\n ]>\r\n <test>\r\n<blob content-encoding=\"base64\"></blob>\r\nThe attribute values of type NOTATION matches one of the notation names included in the declaration;\r\nall notation names in the declaration have been declared\r\n</test>",
  357,
  "<!DOCTYPE test [\n<!NOTATION base64 SYSTEM 'mimecode'>\n<!NOTATION raw SYSTEM 'bin/cat'>\n<!NOTATION uuencode SYSTEM 'uudecode'>\n]>\n<test>&#10;<blob content-encoding=\"base64\"></blob>&#10;The attribute values of type NOTATION matches one of the notation names included in the declaration;&#10;all notation names in the declaration have been declared&#10;</test>"
},
{
  "ibm59v01.xml",
  476,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 59-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT one EMPTY>\r\n <!ELEMENT two EMPTY>\r\n <!ELEMENT enum (#PCDATA)>\r\n <!ATTLIST one attr (one) #IMPLIED>\r\n <!ATTLIST two attr ( enum) #IMPLIED>\r\n <!ATTLIST two attr (one|two) #IMPLIED>\r\n <!ATTLIST two attr (one| two) #IMPLIED>\r\n <!ATTLIST two attr (enum ) #IMPLIED>\r\n <!ATTLIST two attr ( one | two | enum) #IMPLIED>\r\n ]>\r\n <test>\r\nThis is a Positive test\r\n</test>\r\n",
  46,
  "<test>&#10;This is a Positive test&#10;</test>"
},
{
  "ibm59v02.xml",
  385,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 59-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT one EMPTY>\r\n <!ELEMENT two EMPTY>\r\n <!ELEMENT num EMPTY>\r\n <!ATTLIST num value (one|two) #IMPLIED>\r\n ]>\r\n <test>\r\n<num value = \"one\"></num>\r\nThis is a Positive test\r\nThe attribute values of type Enumeration match one of the Nmtoken tokens in the declaration.\r\n</test>",
  171,
  "<test>&#10;<num value=\"one\"></num>&#10;This is a Positive test&#10;The attribute values of type Enumeration match one of the Nmtoken tokens in the declaration.&#10;</test>"
},
{
  "ibm60v01.xml",
  567,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 60-->\r\n<!DOCTYPE Java \r\n [\r\n <!ELEMENT Java (#PCDATA|one|two|three|four)*>\r\n <!ELEMENT one EMPTY>\r\n <!ELEMENT two EMPTY>\r\n <!ELEMENT three EMPTY>\r\n <!ELEMENT four EMPTY>\r\n <!ATTLIST one chapter CDATA #IMPLIED>\r\n <!ATTLIST two chapter CDATA #REQUIRED>\r\n <!ATTLIST three chapter CDATA #FIXED \"JavaBeans\">\r\n <!ATTLIST four chapter CDATA 'defualt'>\r\n ]>\r\n<Java><one chapter=\"Introduction\"/>\r\n <three chapter=\"JavaBeans\"/>\r\n Positive test\r\n DefaultDecl attributes values IMPLIED, REQUIRED, FIXED and default\r\n</Java>",
  184,
  "<Java><one chapter=\"Introduction\"></one>&#10; <three chapter=\"JavaBeans\"></three>&#10; Positive test&#10; DefaultDecl attributes values IMPLIED, REQUIRED, FIXED and default&#10;</Java>"
},
{
  "ibm60v02.xml",
  477,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 60-->\r\n<!DOCTYPE Java \r\n [\r\n <!ELEMENT Java ANY>\r\n <!ELEMENT one EMPTY>\r\n <!ELEMENT two EMPTY>\r\n <!ATTLIST one chapter CDATA #REQUIRED>\r\n <!ATTLIST two chapter CDATA #REQUIRED>\r\n ]>\r\n<Java>\r\n<one chapter=\"Introduction\"></one>\r\n<two chapter=\"JavaApplets\"></two>\r\nPositive test. Required attribute. Every occurrence of an element with a \r\n#REQUIRED attribute default declaration gives the value of that attribute\r\n</Java>",
  251,
  "<Java>&#10;<one chapter=\"Introduction\"></one>&#10;<two chapter=\"JavaApplets\"></two>&#10;Positive test. Required attribute. Every occurrence of an element with a &#10;#REQUIRED attribute default declaration gives the value of that attribute&#10;</Java>"
},
{
  "ibm60v03.xml",
  435,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 60-->\r\n<!DOCTYPE Java \r\n [\r\n <!ELEMENT Java ANY>\r\n <!ELEMENT one EMPTY>\r\n <!ATTLIST one chapter CDATA #FIXED \"Introduction\">\r\n  ]>\r\n<Java>\r\n<one chapter=\"Introduction\"></one>\r\nAn attribute has a default value declared with the #FIXED keyword, \r\nand an instances of that attribute is given a value which is exactly \r\nthe same as the default value in the declaration. \r\n</Java>\r\n",
  258,
  "<Java>&#10;<one chapter=\"Introduction\"></one>&#10;An attribute has a default value declared with the #FIXED keyword, &#10;and an instances of that attribute is given a value which is exactly &#10;the same as the default value in the declaration. &#10;</Java>"
},
{
  "ibm60v04.xml",
  421,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 60-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT a EMPTY>\r\n <!ELEMENT b EMPTY>\r\n <!ELEMENT attr EMPTY>\r\n <!ELEMENT nametoken EMPTY>\r\n <!ATTLIST attr value (a|b) \"a\"> \r\n <!ATTLIST nametoken namevalue NMTOKEN \"hello\">\r\n  ]>\r\n<test>\r\nThe default value specified for an attribute meets the \r\nlexical constraints of the declared attribute type.\r\n</test>\r\n\r\n",
  134,
  "<test>&#10;The default value specified for an attribute meets the &#10;lexical constraints of the declared attribute type.&#10;</test>"
},
{
  "ibm61v01.xml",
  137,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 61-->\r\n<!DOCTYPE animal SYSTEM \"ibm61v01.dtd\">\r\n<animal>\r\n <tiger/>\r\n</animal>\r\n ",
  43,
  "<animal>&#10; <tiger></tiger>&#10;</animal>"
},
{
  "ibm61v02.xml",
  151,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 61-->\r\n<!DOCTYPE animal SYSTEM \"ibm61v02.dtd\"\r\n[\r\n<!ELEMENT animal EMPTY>\r\n]>\r\n<animal/>\r\n\r\n \r\n",
  17,
  "<animal></animal>"
},
{
  "ibm62v01.xml",
  205,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 62-->\r\n<!DOCTYPE animal SYSTEM \"ibm62v01.dtd\">\r\n<animal>\r\n <tiger/>\r\nPositive test. Test includeSect with pattern1 of p62.\r\nNormal Pattern\r\n</animal>",
  120,
  "<animal>&#10; <tiger></tiger>&#10;Positive test. Test includeSect with pattern1 of p62.&#10;Normal Pattern&#10;</animal>"
},
{
  "ibm62v02.xml",
  222,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 62-->\r\n<!DOCTYPE animal SYSTEM \"ibm62v02.dtd\">\r\n<animal>\r\n <tiger/>\r\nPositive test. Test includeSect with pattern2 of p62.\r\nspace included before INCLUDE\r\n</animal>\r\n",
  135,
  "<animal>&#10; <tiger></tiger>&#10;Positive test. Test includeSect with pattern2 of p62.&#10;space included before INCLUDE&#10;</animal>"
},
{
  "ibm62v03.xml",
  221,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 62-->\r\n<!DOCTYPE animal SYSTEM \"ibm62v03.dtd\">\r\n<animal>\r\n <tiger/>\r\nPositive test. Test includeSect with pattern3 of p62.\r\nspace included after INCLUDE\r\n</animal>\r\n",
  134,
  "<animal>&#10; <tiger></tiger>&#10;Positive test. Test includeSect with pattern3 of p62.&#10;space included after INCLUDE&#10;</animal>"
},
{
  "ibm62v04.xml",
  232,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 62-->\r\n<!DOCTYPE animal SYSTEM \"ibm62v04.dtd\">\r\n<animal>\r\n <tiger/>\r\nPositive test. Test includeSect with pattern4 of p62.\r\nspace included before and after INCLUDE\r\n</animal>\r\n",
  145,
  "<animal>&#10; <tiger></tiger>&#10;Positive test. Test includeSect with pattern4 of p62.&#10;space included before and after INCLUDE&#10;</animal>"
},
{
  "ibm62v05.xml",
  242,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 62-->\r\n<!DOCTYPE animal SYSTEM \"ibm62v05.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n<!ELEMENT tiger EMPTY>\r\n]>\r\n\r\n<animal>\r\n <tiger/>\r\nPositive test. Missing external subset declaration.\r\n</animal>",
  99,
  "<animal>&#10; <tiger></tiger>&#10;Positive test. Missing external subset declaration.&#10;</animal>"
},
{
  "ibm63v01.xml",
  280,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 63-->\r\n<!DOCTYPE animal SYSTEM \"ibm63v01.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n<!ELEMENT tiger (#PCDATA)>\r\n<!ATTLIST animal a (tiger) #REQUIRED> \r\n]>\r\n<animal a = \"tiger\">\r\nPositive test. Test for IGNORE with pattern 1.\r\n</animal>",
  83,
  "<animal a=\"tiger\">&#10;Positive test. Test for IGNORE with pattern 1.&#10;</animal>"
},
{
  "ibm63v02.xml",
  280,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 63-->\r\n<!DOCTYPE animal SYSTEM \"ibm63v02.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n<!ELEMENT tiger (#PCDATA)>\r\n<!ATTLIST animal a (tiger) #REQUIRED> \r\n]>\r\n<animal a = \"tiger\">\r\nPositive test. Test for IGNORE with pattern 2.\r\n</animal>",
  83,
  "<animal a=\"tiger\">&#10;Positive test. Test for IGNORE with pattern 2.&#10;</animal>"
},
{
  "ibm63v03.xml",
  280,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 63-->\r\n<!DOCTYPE animal SYSTEM \"ibm63v03.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n<!ELEMENT tiger (#PCDATA)>\r\n<!ATTLIST animal a (tiger) #REQUIRED> \r\n]>\r\n<animal a = \"tiger\">\r\nPositive test. Test for IGNORE with pattern 3.\r\n</animal>",
  83,
  "<animal a=\"tiger\">&#10;Positive test. Test for IGNORE with pattern 3.&#10;</animal>"
},
{
  "ibm63v04.xml",
  280,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 63-->\r\n<!DOCTYPE animal SYSTEM \"ibm63v04.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n<!ELEMENT tiger (#PCDATA)>\r\n<!ATTLIST animal a (tiger) #REQUIRED> \r\n]>\r\n<animal a = \"tiger\">\r\nPositive test. Test for IGNORE with pattern 4.\r\n</animal>",
  83,
  "<animal a=\"tiger\">&#10;Positive test. Test for IGNORE with pattern 4.&#10;</animal>"
},
{
  "ibm63v05.xml",
  280,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 63-->\r\n<!DOCTYPE animal SYSTEM \"ibm63v05.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n<!ELEMENT tiger (#PCDATA)>\r\n<!ATTLIST animal a (tiger) #REQUIRED> \r\n]>\r\n<animal a = \"tiger\">\r\nPositive test. Test for IGNORE with pattern 5.\r\n</animal>",
  83,
  "<animal a=\"tiger\">&#10;Positive test. Test for IGNORE with pattern 5.&#10;</animal>"
},
{
  "ibm64v01.xml",
  177,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 64-->\r\n<!DOCTYPE animal SYSTEM \"ibm64v01.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n]>\r\n<animal>\r\nPositive Test. Pattern1\r\n</animal>",
  50,
  "<animal>&#10;Positive Test. Pattern1&#10;</animal>"
},
{
  "ibm64v02.xml",
  177,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 64-->\r\n<!DOCTYPE animal SYSTEM \"ibm64v02.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n]>\r\n<animal>\r\nPositive Test. Pattern2\r\n</animal>",
  50,
  "<animal>&#10;Positive Test. Pattern2&#10;</animal>"
},
{
  "ibm64v03.xml",
  177,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 64-->\r\n<!DOCTYPE animal SYSTEM \"ibm64v03.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n]>\r\n<animal>\r\nPositive Test. Pattern3\r\n</animal>",
  50,
  "<animal>&#10;Positive Test. Pattern3&#10;</animal>"
},
{
  "ibm65v01.xml",
  192,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 65-->\r\n<!DOCTYPE animal SYSTEM \"ibm65v01.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n]>\r\n<animal>\r\nPositive Test. Pattern1. Empty string.\r\n</animal>",
  65,
  "<animal>&#10;Positive Test. Pattern1. Empty string.&#10;</animal>"
},
{
  "ibm65v02.xml",
  178,
  "<?xml  version=\"1.0\"?>\r\n<!-- syntax test for Production 65-->\r\n<!DOCTYPE animal SYSTEM \"ibm65v02.dtd\"\r\n[\r\n<!ELEMENT animal ANY>\r\n]>\r\n<animal>\r\nPositive Test. Pattern2.\r\n</animal>",
  51,
  "<animal>&#10;Positive Test. Pattern2.&#10;</animal>"
},
{
  "ibm66v01.xml",
  363,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA)>\r\n]>\r\n<root>\r\nTest all valid Charater references for P66:\r\n&#9;&#09;&#0000000009;\r\n&#xA;&#xa;&#x0A;&#x00000000A;\r\n&#x0d;\r\n&#xAB; &#xab; &#xCD; &#xcD; &#xEf; &#xef;\r\n&#67; &#x43; &#x5f;\r\n&#x20; &#xD7A3; &#xAC00;\r\n&#xF900; &#xFFFD;\r\n&#x10000; &#x10FFFF;\r\n</root>\r\n<!--* a valid test for P66 *-->\r\n",
  190,
  "<root>&#10;Test all valid Charater references for P66:&#10;&#9;&#9;&#9;&#10;&#10;&#10;&#10;&#10;&#10;&#13;&#10;\302\253 \302\253 \303\215 \303\215 \303\257 \303\257\46\06110;C C _&#10;  \355\236\243 \352\260\200\46\06110;\357\244\200 \357\277\275\46\06110;\360\220\200\200 \364\217\277\277\46\06110;</root>"
},
{
  "ibm67v01.xml",
  218,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA)>\r\n  <!ATTLIST root attr CDATA #REQUIRED>\r\n  <!ENTITY ge1 \"xyz\">\r\n]>\r\n<root attr=\"&ge1;&#65;\">\r\n&ge1; &#66;\r\n</root>\r\n<!--* a valid test for P67 *-->\r\n",
  40,
  "<root attr=\"xyzA\">&#10;xyz B&#10;</root>"
},
{
  "ibm68v01.xml",
  207,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm68v01.dtd\" [\r\n  <!ELEMENT root (#PCDATA|a)* >\r\n]>\r\n<root>\r\n  pcdata content\r\n  <a attr1=\"xyz\"/>\r\n</root>\r\n<!--* a valid test for P68 VC:Entity Declared *-->\r\n",
  65,
  "<root>&#10;  pcdata content&#10;  <a attr1=\"xyz\"></a>&#10;</root>"
},
{
  "ibm68v02.xml",
  229,
  "<?xml version=\"1.0\" standalone='no'?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA)* >\r\n  <!ENTITY % pe1 SYSTEM \"ibm68v02.ent\">\r\n  %pe1;\r\n]>\r\n<root>\r\n  pcdata content\r\n</root>\r\n<!--* a valid test for P68 VC:Entity Declared *-->\r\n",
  39,
  "<root>&#10;  pcdata content&#10;</root>"
},
{
  "ibm69v01.xml",
  277,
  "<?xml version=\"1.0\" standalone=\"no\" ?>\r\n<!DOCTYPE root SYSTEM \"ibm69v01.dtd\" [\r\n  <!ELEMENT root (#PCDATA|a)* >\r\n  <!ENTITY % pe1 \"<!-- comment in PE -->\">\r\n  %pe1;\r\n]>\r\n<root>\r\n  pcdata content\r\n  <a attr1=\"xyz\"/>\r\n</root>\r\n<!--* a valid test for P69 VC:Entity Declared *-->\r\n",
  65,
  "<root>&#10;  pcdata content&#10;  <a attr1=\"xyz\"></a>&#10;</root>"
},
{
  "ibm69v02.xml",
  229,
  "<?xml version=\"1.0\" standalone='no'?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA)* >\r\n  <!ENTITY % pe1 SYSTEM \"ibm69v02.ent\">\r\n  %pe1;\r\n]>\r\n<root>\r\n  pcdata content\r\n</root>\r\n<!--* a valid test for P69 VC:Entity Declared *-->\r\n",
  39,
  "<root>&#10;  pcdata content&#10;</root>"
},
{
  "ibm70v01.xml",
  478,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root \r\n[\r\n<!ELEMENT root (#PCDATA)>\r\n<!ATTLIST root att CDATA #IMPLIED>\r\n\r\n<!--* Test all legal patterns derived from P70-76 *-->\r\n<!ENTITY % pe1 '<!ATTLIST root att2 CDATA \"&ge1;\">'>\r\n<!ENTITY ge1 \"attdefaultvalue\" >\r\n%pe1;\r\n<!NOTATION JPGformat SYSTEM \"JPGFormat\">\r\n<!ENTITY ge2  SYSTEM \"image.jpg\" NDATA JPGformat>\r\n<!ENTITY % pe2 PUBLIC \"-//w3c//any\" \"ibm70v01.ent\" >\r\n%pe2;\r\n]>\r\n<root att2=\"any\" />\r\n<!-- a valid test case: test P70-P76 -->",
  85,
  "<!DOCTYPE root [\n<!NOTATION JPGformat SYSTEM 'JPGFormat'>\n]>\n<root att2=\"any\"></root>"
},
{
  "ibm78v01.xml",
  365,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root \r\n[\r\n<!ELEMENT root (#PCDATA)>\r\n<!ATTLIST root att CDATA #IMPLIED>\r\n\r\n<!--* Test all legal patterns derived from P78 *-->\r\n<!ENTITY epe1 SYSTEM \"ibm78v01.ent\" >\r\n<!ENTITY epe2 SYSTEM \"ibm78v02.ent\" >\r\n<!ENTITY epe3 SYSTEM \"ibm78v03.ent\" >\r\n\r\n]>\r\n<root>&epe1;&epe2;&epe3;</root>\r\n<!-- a valid test case: test P78, P77, P80 -->\r\n",
  206,
  "<root>&#10;anything legal as PCDATA....&#10;N.B. also tested P77 and P80&#10;anything legal as PCDATA....&#10;N.B. also tested P77anything legal as PCDATA....&#10;e.g. 12345678E-33, &quot;hello&quot;</root>"
},
{
  "ibm79v01.xml",
  250,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE book \r\n[\r\n<!ELEMENT book ANY>\r\n<!ATTLIST notebook att CDATA #IMPLIED>\r\n<!ENTITY % epe SYSTEM \"ibm79v01.ent\" >\r\n%epe;\r\n]>\r\n<book><name>XML Handbook</name> This is a book</book>\r\n\r\n<!-- a valid test case: test P79 -->\r\n",
  53,
  "<book><name>XML Handbook</name> This is a book</book>"
},
{
  "ibm82v01.xml",
  397,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root \r\n[\r\n<!ELEMENT root (#PCDATA)>\r\n<!ATTLIST root att CDATA #IMPLIED>\r\n<!ATTLIST root entatt1 ENTITY #REQUIRED >\r\n\r\n<!--* Test PublicID in P82 *-->\r\n<!NOTATION JPGformat PUBLIC \"-//image//notreal\" >\r\n<!ENTITY unparsed1  SYSTEM \"image.jpg\" NDATA JPGformat>\r\n]>\r\n<root entatt1=\"unparsed1\">test PublicID in P82</root>\r\n<!-- a valid test case: test P82 and P83 -->\r\n",
  122,
  "<!DOCTYPE root [\n<!NOTATION JPGformat PUBLIC '-//image//notreal'>\n]>\n<root entatt1=\"unparsed1\">test PublicID in P82</root>"
},
{
  "ibm85v01.xml",
  4486,
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n<!DOCTYPE book [\r\n<!ELEMENT book ANY>\r\n<!-- This test case covers 149 legal character ranges plus\r\n     51 discrete legal characters for production 85. -->\r\n<?NAME_41-A_5A-Z_4d-M_61-a_7A-z_6d-m_0C0-\303\200\0600D6-\303\226\0600cb-\303\213\0600D8-\303\230\0600F6-\303\266\0600e7-\303\247\0600F8-\303\270\0600FF-\303\277\0600fb-\303\273\061100-\304\200\061131-\304\261\061118-\304\230\061134-\304\264\06113E-\304\276\061139-\304\271\061141-\305\201\061148-\305\210\061144-\305\204\06114A-\305\212\06117E-\305\276\061164-\305\244\061180-\306\200\0611C3-\307\203\0611a1-\306\241\0611CD-\307\215\0611F0-\307\260\0611de-\307\236\0611F4-\307\264\0611F5-\307\265\0611f4-\307\264\0611FA-\307\272\062217-\310\227\062208-\310\210\062250-\311\220\0622A8-\312\250\06227c-\311\274\0622BB-\312\273\0622C1-\313\201\0622be-\312\276\063386-\316\206\063388-\316\210\06338A-\316\212\063389-\316\211\06338C-\316\214\06338E-\316\216\0633A1-\316\241\063397-\316\227\0633A3-\316\243\0633CE-\317\216\0633b8-\316\270\0633D0-\317\220\0633D6-\317\226\0633d3-\317\223\0633DA-\317\232\0633DC-\317\234\0633DE-\317\236\0633E0-\317\240\0633E2-\317\242\0633F3-\317\263\0633ea-\317\252\064401-\320\201\06440C-\320\214\064406-\320\206\06440E-\320\216\06444F-\321\217\06442e-\320\256\064451-\321\221\06445C-\321\234\064456-\321\226\06445E-\321\236\064481-\322\201\06446f-\321\257\064490-\322\220\0644C4-\323\204\0644aa-\322\252\0644C7-\323\207\0644C8-\323\210\0644c7-\323\207\0644CB-\323\213\0644CC-\323\214\0644cb-\323\213\0644D0-\323\220\0644EB-\323\253\0644dd-\323\235\0644EE-\323\256\0644F5-\323\265\0644f1-\323\261\0644F8-\323\270\0644F9-\323\271\0644f8-\323\270\065531-\324\261\065556-\325\226\065543-\325\203\065559-\325\231\065561-\325\241\065586-\326\206\065573-\325\263\0655D0-\327\220\0655EA-\327\252\0655dd-\327\235\0655F0-\327\260\0655F2-\327\262\0655f1-\327\261\066621-\330\241\06663A-\330\272\06662d-\330\255\066641-\331\201\06664A-\331\212\066645-\331\205\066671-\331\261\0666B7-\332\267\066694-\332\224\0666BA-\332\272\0666BE-\332\276\0666bc-\332\274\0666C0-\333\200\0666CE-\333\216\0666c7-\333\207\0666D0-\333\220\0666D3-\333\223\0666d1-\333\221\0666D5-\333\225\0666E5-\333\245\0666E6-\333\246\0666e5-\333\245\0600905-\340\244\205\0600939-\340\244\271\060091f-\340\244\237\060093D-\340\244\275\0600958-\340\245\230\0600961-\340\245\241\060095c-\340\245\234\0600985-\340\246\205\060098C-\340\246\214\0600988-\340\246\210\060098F-\340\246\217\0600990-\340\246\220\060098f-\340\246\217\0600993-\340\246\223\06009A8-\340\246\250\060099d-\340\246\235\06009AA-\340\246\252\06009B0-\340\246\260\06009ad-\340\246\255\06009B2-\340\246\262\06009B6-\340\246\266\06009B9-\340\246\271\06009b7-\340\246\267\06009DC-\340\247\234\06009DD-\340\247\235\06009dc-\340\247\234\06009DF-\340\247\237\06009E1-\340\247\241\06009e0-\340\247\240\06009F0-\340\247\260\06009F1-\340\247\261\06009f0-\340\247\260\0600A05-\340\250\205\0600A0A-\340\250\212\0600a07-\340\250\207\0600A0F-\340\250\217\0600A10-\340\250\220\0600a0f-\340\250\217\0600A13-\340\250\223\0600A28-\340\250\250\0600a1d-\340\250\235\0600A2A-\340\250\252\0600A30-\340\250\260\0600a2d-\340\250\255\0600A32-\340\250\262\0600A33-\340\250\263\0600a32-\340\250\262\0600A35-\340\250\265\0600A36-\340\250\266\0600a35-\340\250\265\0600A38-\340\250\270\0600A39-\340\250\271\0600a38-\340\250\270\0600A59-\340\251\231\0600A5C-\340\251\234\0600a5a-\340\251\232\0600A5E-\340\251\236\0600A72-\340\251\262\0600A74-\340\251\264\0600a73-\340\251\263\0600A85-\340\252\205\0600A8B-\340\252\213\0600a88-\340\252\210\0600A8D-\340\252\215\0600A8F-\340\252\217\0600A91-\340\252\221\0600a90-\340\252\220\0600A93-\340\252\223\0600AA8-\340\252\250\0600a9d-\340\252\235\0600AAA-\340\252\252\0600AB0-\340\252\260\0600aad-\340\252\255\0600AB2-\340\252\262\0600AB3-\340\252\263\0600ab2-\340\252\262\0600AB5-\340\252\265\0600AB9-\340\252\271\0600ab7-\340\252\267\0600ABD-\340\252\275\0600AE0-\340\253\240\0600B05-\340\254\205\0600B0C-\340\254\214\0600b08-\340\254\210\0600B0F-\340\254\217\0600B10-\340\254\220\0600b0f-\340\254\217\0600B13-\340\254\223\0600B28-\340\254\250\0600b1d-\340\254\235\0600B2A-\340\254\252\0600B30-\340\254\260\0600b2d-\340\254\255\0600B32-\340\254\262\0600B33-\340\254\263\0600b32-\340\254\262\0600B36-\340\254\266\0600B39-\340\254\271\0600b37-\340\254\267\0600B3D-\340\254\275\0600B5C-\340\255\234\0600B5D-\340\255\235\0600b5c-\340\255\234\0600B5F-\340\255\237\0600B61-\340\255\241\0600b60-\340\255\240\0600B85-\340\256\205\0600B8A-\340\256\212\0600b87-\340\256\207\0600B8E-\340\256\216\0600B90-\340\256\220\0600b8f-\340\256\217\0600B92-\340\256\222\0600B95-\340\256\225\0600b93-\340\256\223\0600B99-\340\256\231\0600B9A-\340\256\232\0600b99-\340\256\231\0600B9C-\340\256\234\0600B9E-\340\256\236\0600B9F-\340\256\237\0600b9e-\340\256\236\0600BA3-\340\256\243\0600BA4-\340\256\244\0600ba3-\340\256\243\0600BA8-\340\256\250\0600BAA-\340\256\252\0600ba9-\340\256\251\0600BAE-\340\256\256\0600BB5-\340\256\265\0600bb1-\340\256\261\0600BB7-\340\256\267\0600BB9-\340\256\271\0600bb8-\340\256\270\0600C05-\340\260\205\0600C0C-\340\260\214\0600c08-\340\260\210\0600C0E-\340\260\216\0600C10-\340\260\220\0600c0f-\340\260\217\0600C12-\340\260\222\0600C28-\340\260\250\0600c1d-\340\260\235\0600C2A-\340\260\252\0600C33-\340\260\263\0600c2e-\340\260\256\0600C35-\340\260\265\0600C39-\340\260\271\0600c37-\340\260\267\0600C60-\340\261\240\0600C61-\340\261\241\0600c60-\340\261\240\0600C85-\340\262\205\0600C8C-\340\262\214\0600c88-\340\262\210\0600C8E-\340\262\216\0600C90-\340\262\220\0600c8f-\340\262\217\0600C92-\340\262\222\0600CA8-\340\262\250\0600c9d-\340\262\235\0600CAA-\340\262\252\0600CB3-\340\262\263\0600cae-\340\262\256\0600CB5-\340\262\265\0600CB9-\340\262\271\0600cb7-\340\262\267\0600CDE-\340\263\236\0600CE0-\340\263\240\0600CE1-\340\263\241\0600ce0-\340\263\240\0600D05-\340\264\205\0600D0C-\340\264\214\0600d08-\340\264\210\0600D0E-\340\264\216\0600D10-\340\264\220\0600d0f-\340\264\217\0600D12-\340\264\222\0600D28-\340\264\250\0600d1d-\340\264\235\0600D2A-\340\264\252\0600D39-\340\264\271\0600d31-\340\264\261\0600D60-\340\265\240\0600D61-\340\265\241\0600d60-\340\265\240\0600E01-\340\270\201\0600E2E-\340\270\256\0600e17-\340\270\227\0600E30-\340\270\260\0600E32-\340\270\262\0600E33-\340\270\263\0600e32-\340\270\262\0600E40-\340\271\200\0600E45-\340\271\205\0600e42-\340\271\202\0600E81-\340\272\201\0600E82-\340\272\202\0600e81-\340\272\201\0600E84-\340\272\204\0600E87-\340\272\207\0600E88-\340\272\210\0600e87-\340\272\207\0600E8A-\340\272\212\0600E8D-\340\272\215\0600E94-\340\272\224\0600E97-\340\272\227\0600e95-\340\272\225\0600E99-\340\272\231\0600E9F-\340\272\237\0600e9c-\340\272\234\0600EA1-\340\272\241\0600EA3-\340\272\243\0600ea2-\340\272\242\0600EA5-\340\272\245\0600EA7-\340\272\247\0600EAA-\340\272\252\0600EAB-\340\272\253\0600eaa-\340\272\252\0600EAD-\340\272\255\0600EAE-\340\272\256\0600ead-\340\272\255\0600EB0-\340\272\260\0600EB2-\340\272\262\0600EB3-\340\272\263\0600eb2-\340\272\262\0600EBD-\340\272\275\0600EC0-\340\273\200\0600EC4-\340\273\204\0600ec2-\340\273\202\0600F40-\340\275\200\0600F47-\340\275\207\0600f43-\340\275\203\0600F49-\340\275\211\0600F69-\340\275\251\0600f59-\340\275\231\06110A0-\341\202\240\06110C5-\341\203\205\06110b2-\341\202\262\06110D0-\341\203\220\06110F6-\341\203\266\06110e3-\341\203\243\0611100-\341\204\200\0611102-\341\204\202\0611103-\341\204\203\0611102-\341\204\202\0611105-\341\204\205\0611107-\341\204\207\0611106-\341\204\206\0611109-\341\204\211\061110B-\341\204\213\061110C-\341\204\214\061110b-\341\204\213\061110E-\341\204\216\0611112-\341\204\222\0611110-\341\204\220\061113C-\341\204\274\061113E-\341\204\276\0611140-\341\205\200\061114C-\341\205\214\061114E-\341\205\216\0611150-\341\205\220\0611154-\341\205\224\0611155-\341\205\225\0611154-\341\205\224\0611159-\341\205\231\061115F-\341\205\237\0611161-\341\205\241\0611160-\341\205\240\0611163-\341\205\243\0611165-\341\205\245\0611167-\341\205\247\0611169-\341\205\251\061116D-\341\205\255\061116E-\341\205\256\061116d-\341\205\255\0611172-\341\205\262\0611173-\341\205\263\0611172-\341\205\262\0611175-\341\205\265\061119E-\341\206\236\06111A8-\341\206\250\06111AB-\341\206\253\06111AE-\341\206\256\06111AF-\341\206\257\06111ae-\341\206\256\06111B7-\341\206\267\06111B8-\341\206\270\06111b7-\341\206\267\06111BA-\341\206\272\06111BC-\341\206\274\06111C2-\341\207\202\06111bf-\341\206\277\06111EB-\341\207\253\06111F0-\341\207\260\06111F9-\341\207\271\0611E00-\341\270\200\0611E9B-\341\272\233\0611e4d-\341\271\215\0611EA0-\341\272\240\0611EF9-\341\273\271\0611ecc-\341\273\214\0611F00-\341\274\200\0611F15-\341\274\225\0611f0a-\341\274\212\0611F18-\341\274\230\0611F1D-\341\274\235\0611f1a-\341\274\232\0611F20-\341\274\240\0611F45-\341\275\205\0611f32-\341\274\262\0611F48-\341\275\210\0611F4D-\341\275\215\0611f4a-\341\275\212\0611F50-\341\275\220\0611F57-\341\275\227\0611f53-\341\275\223\0611F59-\341\275\231\0611F5B-\341\275\233\0611F5D-\341\275\235\0611F5F-\341\275\237\0611F7D-\341\275\275\0611f6e-\341\275\256\0611F80-\341\276\200\0611FB4-\341\276\264\0611f9a-\341\276\232\0611FB6-\341\276\266\0611FBC-\341\276\274\0611fb9-\341\276\271\0611FBE-\341\276\276\0611FC2-\341\277\202\0611FC4-\341\277\204\0611fc3-\341\277\203\0611FC6-\341\277\206\0611FCC-\341\277\214\0611fc9-\341\277\211\0611FD0-\341\277\220\0611FD3-\341\277\223\0611fd1-\341\277\221\0611FD6-\341\277\226\0611FDB-\341\277\233\0611fd8-\341\277\230\0611FE0-\341\277\240\0611FEC-\341\277\254\0611fe6-\341\277\246\0611FF2-\341\277\262\0611FF4-\341\277\264\0611ff3-\341\277\263\0611FF6-\341\277\266\0611FFC-\341\277\274\0611ff9-\341\277\271\0622126-\342\204\246\062212A-\342\204\252\062212B-\342\204\253\062212a-\342\204\252\062212E-\342\204\256\0622180-\342\206\200\0622182-\342\206\202\0622181-\342\206\201\0633041-\343\201\201\0633094-\343\202\224\063306a-\343\201\252\06330A1-\343\202\241\06330FA-\343\203\272\06330cd-\343\203\215\0633105-\343\204\205\063312C-\343\204\254\0633118-\343\204\230\137\101\06000-\352\260\200\137\0677A3-\355\236\243\137\0611d1-\354\207\221 This is a PI target ?>\r\n]>\r\n<book/>\r\n",
  4286,
  "<?NAME_41-A_5A-Z_4d-M_61-a_7A-z_6d-m_0C0-\303\200\0600D6-\303\226\0600cb-\303\213\0600D8-\303\230\0600F6-\303\266\0600e7-\303\247\0600F8-\303\270\0600FF-\303\277\0600fb-\303\273\061100-\304\200\061131-\304\261\061118-\304\230\061134-\304\264\06113E-\304\276\061139-\304\271\061141-\305\201\061148-\305\210\061144-\305\204\06114A-\305\212\06117E-\305\276\061164-\305\244\061180-\306\200\0611C3-\307\203\0611a1-\306\241\0611CD-\307\215\0611F0-\307\260\0611de-\307\236\0611F4-\307\264\0611F5-\307\265\0611f4-\307\264\0611FA-\307\272\062217-\310\227\062208-\310\210\062250-\311\220\0622A8-\312\250\06227c-\311\274\0622BB-\312\273\0622C1-\313\201\0622be-\312\276\063386-\316\206\063388-\316\210\06338A-\316\212\063389-\316\211\06338C-\316\214\06338E-\316\216\0633A1-\316\241\063397-\316\227\0633A3-\316\243\0633CE-\317\216\0633b8-\316\270\0633D0-\317\220\0633D6-\317\226\0633d3-\317\223\0633DA-\317\232\0633DC-\317\234\0633DE-\317\236\0633E0-\317\240\0633E2-\317\242\0633F3-\317\263\0633ea-\317\252\064401-\320\201\06440C-\320\214\064406-\320\206\06440E-\320\216\06444F-\321\217\06442e-\320\256\064451-\321\221\06445C-\321\234\064456-\321\226\06445E-\321\236\064481-\322\201\06446f-\321\257\064490-\322\220\0644C4-\323\204\0644aa-\322\252\0644C7-\323\207\0644C8-\323\210\0644c7-\323\207\0644CB-\323\213\0644CC-\323\214\0644cb-\323\213\0644D0-\323\220\0644EB-\323\253\0644dd-\323\235\0644EE-\323\256\0644F5-\323\265\0644f1-\323\261\0644F8-\323\270\0644F9-\323\271\0644f8-\323\270\065531-\324\261\065556-\325\226\065543-\325\203\065559-\325\231\065561-\325\241\065586-\326\206\065573-\325\263\0655D0-\327\220\0655EA-\327\252\0655dd-\327\235\0655F0-\327\260\0655F2-\327\262\0655f1-\327\261\066621-\330\241\06663A-\330\272\06662d-\330\255\066641-\331\201\06664A-\331\212\066645-\331\205\066671-\331\261\0666B7-\332\267\066694-\332\224\0666BA-\332\272\0666BE-\332\276\0666bc-\332\274\0666C0-\333\200\0666CE-\333\216\0666c7-\333\207\0666D0-\333\220\0666D3-\333\223\0666d1-\333\221\0666D5-\333\225\0666E5-\333\245\0666E6-\333\246\0666e5-\333\245\0600905-\340\244\205\0600939-\340\244\271\060091f-\340\244\237\060093D-\340\244\275\0600958-\340\245\230\0600961-\340\245\241\060095c-\340\245\234\0600985-\340\246\205\060098C-\340\246\214\0600988-\340\246\210\060098F-\340\246\217\0600990-\340\246\220\060098f-\340\246\217\0600993-\340\246\223\06009A8-\340\246\250\060099d-\340\246\235\06009AA-\340\246\252\06009B0-\340\246\260\06009ad-\340\246\255\06009B2-\340\246\262\06009B6-\340\246\266\06009B9-\340\246\271\06009b7-\340\246\267\06009DC-\340\247\234\06009DD-\340\247\235\06009dc-\340\247\234\06009DF-\340\247\237\06009E1-\340\247\241\06009e0-\340\247\240\06009F0-\340\247\260\06009F1-\340\247\261\06009f0-\340\247\260\0600A05-\340\250\205\0600A0A-\340\250\212\0600a07-\340\250\207\0600A0F-\340\250\217\0600A10-\340\250\220\0600a0f-\340\250\217\0600A13-\340\250\223\0600A28-\340\250\250\0600a1d-\340\250\235\0600A2A-\340\250\252\0600A30-\340\250\260\0600a2d-\340\250\255\0600A32-\340\250\262\0600A33-\340\250\263\0600a32-\340\250\262\0600A35-\340\250\265\0600A36-\340\250\266\0600a35-\340\250\265\0600A38-\340\250\270\0600A39-\340\250\271\0600a38-\340\250\270\0600A59-\340\251\231\0600A5C-\340\251\234\0600a5a-\340\251\232\0600A5E-\340\251\236\0600A72-\340\251\262\0600A74-\340\251\264\0600a73-\340\251\263\0600A85-\340\252\205\0600A8B-\340\252\213\0600a88-\340\252\210\0600A8D-\340\252\215\0600A8F-\340\252\217\0600A91-\340\252\221\0600a90-\340\252\220\0600A93-\340\252\223\0600AA8-\340\252\250\0600a9d-\340\252\235\0600AAA-\340\252\252\0600AB0-\340\252\260\0600aad-\340\252\255\0600AB2-\340\252\262\0600AB3-\340\252\263\0600ab2-\340\252\262\0600AB5-\340\252\265\0600AB9-\340\252\271\0600ab7-\340\252\267\0600ABD-\340\252\275\0600AE0-\340\253\240\0600B05-\340\254\205\0600B0C-\340\254\214\0600b08-\340\254\210\0600B0F-\340\254\217\0600B10-\340\254\220\0600b0f-\340\254\217\0600B13-\340\254\223\0600B28-\340\254\250\0600b1d-\340\254\235\0600B2A-\340\254\252\0600B30-\340\254\260\0600b2d-\340\254\255\0600B32-\340\254\262\0600B33-\340\254\263\0600b32-\340\254\262\0600B36-\340\254\266\0600B39-\340\254\271\0600b37-\340\254\267\0600B3D-\340\254\275\0600B5C-\340\255\234\0600B5D-\340\255\235\0600b5c-\340\255\234\0600B5F-\340\255\237\0600B61-\340\255\241\0600b60-\340\255\240\0600B85-\340\256\205\0600B8A-\340\256\212\0600b87-\340\256\207\0600B8E-\340\256\216\0600B90-\340\256\220\0600b8f-\340\256\217\0600B92-\340\256\222\0600B95-\340\256\225\0600b93-\340\256\223\0600B99-\340\256\231\0600B9A-\340\256\232\0600b99-\340\256\231\0600B9C-\340\256\234\0600B9E-\340\256\236\0600B9F-\340\256\237\0600b9e-\340\256\236\0600BA3-\340\256\243\0600BA4-\340\256\244\0600ba3-\340\256\243\0600BA8-\340\256\250\0600BAA-\340\256\252\0600ba9-\340\256\251\0600BAE-\340\256\256\0600BB5-\340\256\265\0600bb1-\340\256\261\0600BB7-\340\256\267\0600BB9-\340\256\271\0600bb8-\340\256\270\0600C05-\340\260\205\0600C0C-\340\260\214\0600c08-\340\260\210\0600C0E-\340\260\216\0600C10-\340\260\220\0600c0f-\340\260\217\0600C12-\340\260\222\0600C28-\340\260\250\0600c1d-\340\260\235\0600C2A-\340\260\252\0600C33-\340\260\263\0600c2e-\340\260\256\0600C35-\340\260\265\0600C39-\340\260\271\0600c37-\340\260\267\0600C60-\340\261\240\0600C61-\340\261\241\0600c60-\340\261\240\0600C85-\340\262\205\0600C8C-\340\262\214\0600c88-\340\262\210\0600C8E-\340\262\216\0600C90-\340\262\220\0600c8f-\340\262\217\0600C92-\340\262\222\0600CA8-\340\262\250\0600c9d-\340\262\235\0600CAA-\340\262\252\0600CB3-\340\262\263\0600cae-\340\262\256\0600CB5-\340\262\265\0600CB9-\340\262\271\0600cb7-\340\262\267\0600CDE-\340\263\236\0600CE0-\340\263\240\0600CE1-\340\263\241\0600ce0-\340\263\240\0600D05-\340\264\205\0600D0C-\340\264\214\0600d08-\340\264\210\0600D0E-\340\264\216\0600D10-\340\264\220\0600d0f-\340\264\217\0600D12-\340\264\222\0600D28-\340\264\250\0600d1d-\340\264\235\0600D2A-\340\264\252\0600D39-\340\264\271\0600d31-\340\264\261\0600D60-\340\265\240\0600D61-\340\265\241\0600d60-\340\265\240\0600E01-\340\270\201\0600E2E-\340\270\256\0600e17-\340\270\227\0600E30-\340\270\260\0600E32-\340\270\262\0600E33-\340\270\263\0600e32-\340\270\262\0600E40-\340\271\200\0600E45-\340\271\205\0600e42-\340\271\202\0600E81-\340\272\201\0600E82-\340\272\202\0600e81-\340\272\201\0600E84-\340\272\204\0600E87-\340\272\207\0600E88-\340\272\210\0600e87-\340\272\207\0600E8A-\340\272\212\0600E8D-\340\272\215\0600E94-\340\272\224\0600E97-\340\272\227\0600e95-\340\272\225\0600E99-\340\272\231\0600E9F-\340\272\237\0600e9c-\340\272\234\0600EA1-\340\272\241\0600EA3-\340\272\243\0600ea2-\340\272\242\0600EA5-\340\272\245\0600EA7-\340\272\247\0600EAA-\340\272\252\0600EAB-\340\272\253\0600eaa-\340\272\252\0600EAD-\340\272\255\0600EAE-\340\272\256\0600ead-\340\272\255\0600EB0-\340\272\260\0600EB2-\340\272\262\0600EB3-\340\272\263\0600eb2-\340\272\262\0600EBD-\340\272\275\0600EC0-\340\273\200\0600EC4-\340\273\204\0600ec2-\340\273\202\0600F40-\340\275\200\0600F47-\340\275\207\0600f43-\340\275\203\0600F49-\340\275\211\0600F69-\340\275\251\0600f59-\340\275\231\06110A0-\341\202\240\06110C5-\341\203\205\06110b2-\341\202\262\06110D0-\341\203\220\06110F6-\341\203\266\06110e3-\341\203\243\0611100-\341\204\200\0611102-\341\204\202\0611103-\341\204\203\0611102-\341\204\202\0611105-\341\204\205\0611107-\341\204\207\0611106-\341\204\206\0611109-\341\204\211\061110B-\341\204\213\061110C-\341\204\214\061110b-\341\204\213\061110E-\341\204\216\0611112-\341\204\222\0611110-\341\204\220\061113C-\341\204\274\061113E-\341\204\276\0611140-\341\205\200\061114C-\341\205\214\061114E-\341\205\216\0611150-\341\205\220\0611154-\341\205\224\0611155-\341\205\225\0611154-\341\205\224\0611159-\341\205\231\061115F-\341\205\237\0611161-\341\205\241\0611160-\341\205\240\0611163-\341\205\243\0611165-\341\205\245\0611167-\341\205\247\0611169-\341\205\251\061116D-\341\205\255\061116E-\341\205\256\061116d-\341\205\255\0611172-\341\205\262\0611173-\341\205\263\0611172-\341\205\262\0611175-\341\205\265\061119E-\341\206\236\06111A8-\341\206\250\06111AB-\341\206\253\06111AE-\341\206\256\06111AF-\341\206\257\06111ae-\341\206\256\06111B7-\341\206\267\06111B8-\341\206\270\06111b7-\341\206\267\06111BA-\341\206\272\06111BC-\341\206\274\06111C2-\341\207\202\06111bf-\341\206\277\06111EB-\341\207\253\06111F0-\341\207\260\06111F9-\341\207\271\0611E00-\341\270\200\0611E9B-\341\272\233\0611e4d-\341\271\215\0611EA0-\341\272\240\0611EF9-\341\273\271\0611ecc-\341\273\214\0611F00-\341\274\200\0611F15-\341\274\225\0611f0a-\341\274\212\0611F18-\341\274\230\0611F1D-\341\274\235\0611f1a-\341\274\232\0611F20-\341\274\240\0611F45-\341\275\205\0611f32-\341\274\262\0611F48-\341\275\210\0611F4D-\341\275\215\0611f4a-\341\275\212\0611F50-\341\275\220\0611F57-\341\275\227\0611f53-\341\275\223\0611F59-\341\275\231\0611F5B-\341\275\233\0611F5D-\341\275\235\0611F5F-\341\275\237\0611F7D-\341\275\275\0611f6e-\341\275\256\0611F80-\341\276\200\0611FB4-\341\276\264\0611f9a-\341\276\232\0611FB6-\341\276\266\0611FBC-\341\276\274\0611fb9-\341\276\271\0611FBE-\341\276\276\0611FC2-\341\277\202\0611FC4-\341\277\204\0611fc3-\341\277\203\0611FC6-\341\277\206\0611FCC-\341\277\214\0611fc9-\341\277\211\0611FD0-\341\277\220\0611FD3-\341\277\223\0611fd1-\341\277\221\0611FD6-\341\277\226\0611FDB-\341\277\233\0611fd8-\341\277\230\0611FE0-\341\277\240\0611FEC-\341\277\254\0611fe6-\341\277\246\0611FF2-\341\277\262\0611FF4-\341\277\264\0611ff3-\341\277\263\0611FF6-\341\277\266\0611FFC-\341\277\274\0611ff9-\341\277\271\0622126-\342\204\246\062212A-\342\204\252\062212B-\342\204\253\062212a-\342\204\252\062212E-\342\204\256\0622180-\342\206\200\0622182-\342\206\202\0622181-\342\206\201\0633041-\343\201\201\0633094-\343\202\224\063306a-\343\201\252\06330A1-\343\202\241\06330FA-\343\203\272\06330cd-\343\203\215\0633105-\343\204\205\063312C-\343\204\254\0633118-\343\204\230\137\101\06000-\352\260\200\137\0677A3-\355\236\243\137\0611d1-\354\207\221 This is a PI target ?><book></book>"
},
{
  "ibm86v01.xml",
  302,
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n<!DOCTYPE book [\r\n<!ELEMENT book ANY>\r\n<!-- This test case covers 2 legal character ranges plus\r\n     1 discrete legal characters for production 86. -->\r\n<?NAME_4E00-\344\270\200\0719FA5-\351\276\245\06776d2-\347\233\222\0633007-\343\200\207\0633021-\343\200\241\0633029-\343\200\251\0633025-\343\200\245 This is a PI target ?>\r\n]>\r\n<book/>\r\n",
  105,
  "<?NAME_4E00-\344\270\200\0719FA5-\351\276\245\06776d2-\347\233\222\0633007-\343\200\207\0633021-\343\200\241\0633029-\343\200\251\0633025-\343\200\245 This is a PI target ?><book></book>"
},
{
  "ibm87v01.xml",
  2200,
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n<!DOCTYPE book [\r\n<!ELEMENT book ANY>\r\n<!-- This test case covers 65 legal character ranges plus\r\n     30 discrete legal characters for production 87. -->\r\n<?NAME_300-\314\200\063345-\315\205\063322-\314\242\063360-\315\240\063361-\315\241\063360-\315\240\064483-\322\203\064486-\322\206\064484-\322\204\065591-\326\221\0655A1-\326\241\065599-\326\231\0655A3-\326\243\0655B9-\326\271\0655ae-\326\256\0655BB-\326\273\0655BD-\326\275\0655bc-\326\274\0655BF-\326\277\0655C1-\327\201\0655C2-\327\202\0655c1-\327\201\0655C4-\327\204\06664B-\331\213\066652-\331\222\06664e-\331\216\066670-\331\260\0666D6-\333\226\0666DC-\333\234\0666d9-\333\231\0666DD-\333\235\0666DF-\333\237\0666de-\333\236\0666E0-\333\240\0666E4-\333\244\0666e2-\333\242\0666E7-\333\247\0666E8-\333\250\0666e7-\333\247\0666EA-\333\252\0666ED-\333\255\0666eb-\333\253\0600901-\340\244\201\0600903-\340\244\203\0600902-\340\244\202\060093C-\340\244\274\060093E-\340\244\276\060094C-\340\245\214\0600945-\340\245\205\060094D-\340\245\215\0600951-\340\245\221\0600954-\340\245\224\0600952-\340\245\222\0600962-\340\245\242\0600963-\340\245\243\0600962-\340\245\242\0600981-\340\246\201\0600983-\340\246\203\0600982-\340\246\202\06009BC-\340\246\274\06009BE-\340\246\276\06009BF-\340\246\277\06009C0-\340\247\200\06009C4-\340\247\204\06009c2-\340\247\202\06009C7-\340\247\207\06009C8-\340\247\210\06009c7-\340\247\207\06009CB-\340\247\213\06009CD-\340\247\215\06009cc-\340\247\214\06009D7-\340\247\227\06009E2-\340\247\242\06009E3-\340\247\243\06009e2-\340\247\242\0600A02-\340\250\202\0600A3C-\340\250\274\0600A3E-\340\250\276\0600A3F-\340\250\277\0600A40-\340\251\200\0600A42-\340\251\202\0600a41-\340\251\201\0600A47-\340\251\207\0600A48-\340\251\210\0600a47-\340\251\207\0600A4B-\340\251\213\0600A4D-\340\251\215\0600a4c-\340\251\214\0600A70-\340\251\260\0600A71-\340\251\261\0600a70-\340\251\260\0600A81-\340\252\201\0600A83-\340\252\203\0600a82-\340\252\202\0600ABC-\340\252\274\0600ABE-\340\252\276\0600AC5-\340\253\205\0600ac1-\340\253\201\0600AC7-\340\253\207\0600AC9-\340\253\211\0600ac8-\340\253\210\0600ACB-\340\253\213\0600ACD-\340\253\215\0600acc-\340\253\214\0600B01-\340\254\201\0600B03-\340\254\203\0600b02-\340\254\202\0600B3C-\340\254\274\0600B3E-\340\254\276\0600B43-\340\255\203\0600b40-\340\255\200\0600B47-\340\255\207\0600B48-\340\255\210\0600b47-\340\255\207\0600B4B-\340\255\213\0600B4D-\340\255\215\0600b4c-\340\255\214\0600B56-\340\255\226\0600B57-\340\255\227\0600b56-\340\255\226\0600B82-\340\256\202\0600B83-\340\256\203\0600b82-\340\256\202\0600BBE-\340\256\276\0600BC2-\340\257\202\0600bc0-\340\257\200\0600BC6-\340\257\206\0600BC8-\340\257\210\0600bc7-\340\257\207\0600BCA-\340\257\212\0600BCD-\340\257\215\0600bcb-\340\257\213\0600BD7-\340\257\227\0600C01-\340\260\201\0600C03-\340\260\203\0600c02-\340\260\202\0600C3E-\340\260\276\0600C44-\340\261\204\0600c41-\340\261\201\0600C46-\340\261\206\0600C48-\340\261\210\0600c47-\340\261\207\0600C4A-\340\261\212\0600C4D-\340\261\215\0600c4b-\340\261\213\0600C55-\340\261\225\0600C56-\340\261\226\0600c55-\340\261\225\0600C82-\340\262\202\0600C83-\340\262\203\0600c82-\340\262\202\0600CBE-\340\262\276\0600CC4-\340\263\204\0600cc1-\340\263\201\0600CC6-\340\263\206\0600CC8-\340\263\210\0600cc7-\340\263\207\0600CCA-\340\263\212\0600CCD-\340\263\215\0600ccb-\340\263\213\0600CD5-\340\263\225\0600CD6-\340\263\226\0600cd5-\340\263\225\0600D02-\340\264\202\0600D03-\340\264\203\0600d02-\340\264\202\0600D3E-\340\264\276\0600D43-\340\265\203\0600d40-\340\265\200\0600D46-\340\265\206\0600D48-\340\265\210\0600d47-\340\265\207\0600D4A-\340\265\212\0600D4D-\340\265\215\0600d4b-\340\265\213\0600D57-\340\265\227\0600E31-\340\270\261\0600E34-\340\270\264\0600E3A-\340\270\272\0600e37-\340\270\267\0600E47-\340\271\207\0600E4E-\340\271\216\0600e4a-\340\271\212\0600EB1-\340\272\261\0600EB4-\340\272\264\0600EB9-\340\272\271\0600eb6-\340\272\266\0600EBB-\340\272\273\0600EBC-\340\272\274\0600ebb-\340\272\273\0600EC8-\340\273\210\0600ECD-\340\273\215\0600eca-\340\273\212\0600F18-\340\274\230\0600F19-\340\274\231\0600f18-\340\274\230\0600F35-\340\274\265\0600F37-\340\274\267\0600F39-\340\274\271\0600F3E-\340\274\276\0600F3F-\340\274\277\0600F71-\340\275\261\0600F84-\340\276\204\0600f7a-\340\275\272\0600F86-\340\276\206\0600F8B-\340\276\213\0600f88-\340\276\210\0600F90-\340\276\220\0600F95-\340\276\225\0600f92-\340\276\222\0600F97-\340\276\227\0600F99-\340\276\231\0600FAD-\340\276\255\0600fa3-\340\276\243\0600FB1-\340\276\261\0600FB7-\340\276\267\0600fb4-\340\276\264\0600FB9-\340\276\271\06220D0-\342\203\220\06220DC-\342\203\234\06220d6-\342\203\226\06220E1-\342\203\241\063302A-\343\200\252\063302F-\343\200\257\063302c-\343\200\254\0633099-\343\202\231\063309A-\343\202\232 This is a PI target ?>\r\n]>\r\n<book/>\r\n",
  2001,
  "<?NAME_300-\314\200\063345-\315\205\063322-\314\242\063360-\315\240\063361-\315\241\063360-\315\240\064483-\322\203\064486-\322\206\064484-\322\204\065591-\326\221\0655A1-\326\241\065599-\326\231\0655A3-\326\243\0655B9-\326\271\0655ae-\326\256\0655BB-\326\273\0655BD-\326\275\0655bc-\326\274\0655BF-\326\277\0655C1-\327\201\0655C2-\327\202\0655c1-\327\201\0655C4-\327\204\06664B-\331\213\066652-\331\222\06664e-\331\216\066670-\331\260\0666D6-\333\226\0666DC-\333\234\0666d9-\333\231\0666DD-\333\235\0666DF-\333\237\0666de-\333\236\0666E0-\333\240\0666E4-\333\244\0666e2-\333\242\0666E7-\333\247\0666E8-\333\250\0666e7-\333\247\0666EA-\333\252\0666ED-\333\255\0666eb-\333\253\0600901-\340\244\201\0600903-\340\244\203\0600902-\340\244\202\060093C-\340\244\274\060093E-\340\244\276\060094C-\340\245\214\0600945-\340\245\205\060094D-\340\245\215\0600951-\340\245\221\0600954-\340\245\224\0600952-\340\245\222\0600962-\340\245\242\0600963-\340\245\243\0600962-\340\245\242\0600981-\340\246\201\0600983-\340\246\203\0600982-\340\246\202\06009BC-\340\246\274\06009BE-\340\246\276\06009BF-\340\246\277\06009C0-\340\247\200\06009C4-\340\247\204\06009c2-\340\247\202\06009C7-\340\247\207\06009C8-\340\247\210\06009c7-\340\247\207\06009CB-\340\247\213\06009CD-\340\247\215\06009cc-\340\247\214\06009D7-\340\247\227\06009E2-\340\247\242\06009E3-\340\247\243\06009e2-\340\247\242\0600A02-\340\250\202\0600A3C-\340\250\274\0600A3E-\340\250\276\0600A3F-\340\250\277\0600A40-\340\251\200\0600A42-\340\251\202\0600a41-\340\251\201\0600A47-\340\251\207\0600A48-\340\251\210\0600a47-\340\251\207\0600A4B-\340\251\213\0600A4D-\340\251\215\0600a4c-\340\251\214\0600A70-\340\251\260\0600A71-\340\251\261\0600a70-\340\251\260\0600A81-\340\252\201\0600A83-\340\252\203\0600a82-\340\252\202\0600ABC-\340\252\274\0600ABE-\340\252\276\0600AC5-\340\253\205\0600ac1-\340\253\201\0600AC7-\340\253\207\0600AC9-\340\253\211\0600ac8-\340\253\210\0600ACB-\340\253\213\0600ACD-\340\253\215\0600acc-\340\253\214\0600B01-\340\254\201\0600B03-\340\254\203\0600b02-\340\254\202\0600B3C-\340\254\274\0600B3E-\340\254\276\0600B43-\340\255\203\0600b40-\340\255\200\0600B47-\340\255\207\0600B48-\340\255\210\0600b47-\340\255\207\0600B4B-\340\255\213\0600B4D-\340\255\215\0600b4c-\340\255\214\0600B56-\340\255\226\0600B57-\340\255\227\0600b56-\340\255\226\0600B82-\340\256\202\0600B83-\340\256\203\0600b82-\340\256\202\0600BBE-\340\256\276\0600BC2-\340\257\202\0600bc0-\340\257\200\0600BC6-\340\257\206\0600BC8-\340\257\210\0600bc7-\340\257\207\0600BCA-\340\257\212\0600BCD-\340\257\215\0600bcb-\340\257\213\0600BD7-\340\257\227\0600C01-\340\260\201\0600C03-\340\260\203\0600c02-\340\260\202\0600C3E-\340\260\276\0600C44-\340\261\204\0600c41-\340\261\201\0600C46-\340\261\206\0600C48-\340\261\210\0600c47-\340\261\207\0600C4A-\340\261\212\0600C4D-\340\261\215\0600c4b-\340\261\213\0600C55-\340\261\225\0600C56-\340\261\226\0600c55-\340\261\225\0600C82-\340\262\202\0600C83-\340\262\203\0600c82-\340\262\202\0600CBE-\340\262\276\0600CC4-\340\263\204\0600cc1-\340\263\201\0600CC6-\340\263\206\0600CC8-\340\263\210\0600cc7-\340\263\207\0600CCA-\340\263\212\0600CCD-\340\263\215\0600ccb-\340\263\213\0600CD5-\340\263\225\0600CD6-\340\263\226\0600cd5-\340\263\225\0600D02-\340\264\202\0600D03-\340\264\203\0600d02-\340\264\202\0600D3E-\340\264\276\0600D43-\340\265\203\0600d40-\340\265\200\0600D46-\340\265\206\0600D48-\340\265\210\0600d47-\340\265\207\0600D4A-\340\265\212\0600D4D-\340\265\215\0600d4b-\340\265\213\0600D57-\340\265\227\0600E31-\340\270\261\0600E34-\340\270\264\0600E3A-\340\270\272\0600e37-\340\270\267\0600E47-\340\271\207\0600E4E-\340\271\216\0600e4a-\340\271\212\0600EB1-\340\272\261\0600EB4-\340\272\264\0600EB9-\340\272\271\0600eb6-\340\272\266\0600EBB-\340\272\273\0600EBC-\340\272\274\0600ebb-\340\272\273\0600EC8-\340\273\210\0600ECD-\340\273\215\0600eca-\340\273\212\0600F18-\340\274\230\0600F19-\340\274\231\0600f18-\340\274\230\0600F35-\340\274\265\0600F37-\340\274\267\0600F39-\340\274\271\0600F3E-\340\274\276\0600F3F-\340\274\277\0600F71-\340\275\261\0600F84-\340\276\204\0600f7a-\340\275\272\0600F86-\340\276\206\0600F8B-\340\276\213\0600f88-\340\276\210\0600F90-\340\276\220\0600F95-\340\276\225\0600f92-\340\276\222\0600F97-\340\276\227\0600F99-\340\276\231\0600FAD-\340\276\255\0600fa3-\340\276\243\0600FB1-\340\276\261\0600FB7-\340\276\267\0600fb4-\340\276\264\0600FB9-\340\276\271\06220D0-\342\203\220\06220DC-\342\203\234\06220d6-\342\203\226\06220E1-\342\203\241\063302A-\343\200\252\063302F-\343\200\257\063302c-\343\200\254\0633099-\343\202\231\063309A-\343\202\232 This is a PI target ?><book></book>"
},
{
  "ibm88v01.xml",
  621,
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n<!DOCTYPE book [\r\n<!ELEMENT book ANY>\r\n<!-- This test case covers 15 legal character ranges plus\r\n     0 discrete legal characters for production 88. -->\r\n<?NAME_30-0_39-9_34-4_660-\331\240\066669-\331\251\066664-\331\244\0666F0-\333\260\0666F9-\333\271\0666f4-\333\264\0600966-\340\245\246\060096F-\340\245\257\060096a-\340\245\252\06009E6-\340\247\246\06009EF-\340\247\257\06009ea-\340\247\252\0600A66-\340\251\246\0600A6F-\340\251\257\0600a6a-\340\251\252\0600AE6-\340\253\246\0600AEF-\340\253\257\0600aea-\340\253\252\0600B66-\340\255\246\0600B6F-\340\255\257\0600b6a-\340\255\252\0600BE7-\340\257\247\0600BEF-\340\257\257\0600beb-\340\257\253\0600C66-\340\261\246\0600C6F-\340\261\257\0600c6a-\340\261\252\0600CE6-\340\263\246\0600CEF-\340\263\257\0600cea-\340\263\252\0600D66-\340\265\246\0600D6F-\340\265\257\0600d6a-\340\265\252\0600E50-\340\271\220\0600E59-\340\271\231\0600e54-\340\271\224\0600ED0-\340\273\220\0600ED9-\340\273\231\0600ed4-\340\273\224\0600F20-\340\274\240\0600F29-\340\274\251\0600f24-\340\274\244 This is a PI target ?>\r\n]>\r\n<book/>\r\n",
  423,
  "<?NAME_30-0_39-9_34-4_660-\331\240\066669-\331\251\066664-\331\244\0666F0-\333\260\0666F9-\333\271\0666f4-\333\264\0600966-\340\245\246\060096F-\340\245\257\060096a-\340\245\252\06009E6-\340\247\246\06009EF-\340\247\257\06009ea-\340\247\252\0600A66-\340\251\246\0600A6F-\340\251\257\0600a6a-\340\251\252\0600AE6-\340\253\246\0600AEF-\340\253\257\0600aea-\340\253\252\0600B66-\340\255\246\0600B6F-\340\255\257\0600b6a-\340\255\252\0600BE7-\340\257\247\0600BEF-\340\257\257\0600beb-\340\257\253\0600C66-\340\261\246\0600C6F-\340\261\257\0600c6a-\340\261\252\0600CE6-\340\263\246\0600CEF-\340\263\257\0600cea-\340\263\252\0600D66-\340\265\246\0600D6F-\340\265\257\0600d6a-\340\265\252\0600E50-\340\271\220\0600E59-\340\271\231\0600e54-\340\271\224\0600ED0-\340\273\220\0600ED9-\340\273\231\0600ed4-\340\273\224\0600F20-\340\274\240\0600F29-\340\274\251\0600f24-\340\274\244 This is a PI target ?><book></book>"
},
{
  "ibm89v01.xml",
  382,
  "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n<!DOCTYPE book [\r\n<!ELEMENT book ANY>\r\n<!-- This test case covers 3 legal character ranges plus\r\n     8 discrete legal characters for production 89. -->\r\n<?NAME_0B7-\302\267\0622D0-\313\220\0622D1-\313\221\063387-\316\207\066640-\331\200\0600E46-\340\271\206\0600EC6-\340\273\206\0633005-\343\200\205\0633031-\343\200\261\0633035-\343\200\265\0633033-\343\200\263\063309D-\343\202\235\063309E-\343\202\236\063309d-\343\202\235\06330FC-\343\203\274\06330FE-\343\203\276\06330fd-\343\203\275 This is a PI target ?>\r\n]>\r\n<book/>\r\n",
  185,
  "<?NAME_0B7-\302\267\0622D0-\313\220\0622D1-\313\221\063387-\316\207\066640-\331\200\0600E46-\340\271\206\0600EC6-\340\273\206\0633005-\343\200\205\0633031-\343\200\261\0633035-\343\200\265\0633033-\343\200\263\063309D-\343\202\235\063309E-\343\202\236\063309d-\343\202\235\06330FC-\343\203\274\06330FE-\343\203\276\06330fd-\343\203\275 This is a PI target ?><book></book>"
},
{
  "ibm28i01.xml",
  231,
  "<?xml version=\"1.0\" encoding='UTF-8'?>\r\n<!DOCTYPE tiger [\r\n   <!ELEMENT tiger EMPTY>\r\n]>\r\n<!-- This against VC of P28. The Name in the document type declaration\r\n does not match the element type of the root element. --> \r\n<animal/>",
  0, NULL
},
{
  "ibm32i01.xml",
  434,
  "<?xml version=\"1.0\" standalone=\"yes\" ?>\r\n<!DOCTYPE animal SYSTEM \"ibm32i01.dtd\" [\r\n   <!ELEMENT animal EMPTY>\r\n]>\r\n<!-- This is against VC: Standalone Document Declaration in P32\r\n The standalone document declaration has the value \"yes\", there is an external \r\n markup declaration of attributes with default values, and the associated \r\n element appears in the document with specified values for those attributes.   \r\n-->\r\n<animal/>\r\n",
  0, NULL
},
{
  "ibm39i01.xml",
  283,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (a,b)>\r\n  <!ELEMENT a EMPTY>\r\n  <!ELEMENT b (#PCDATA|c)* >\r\n  <!ELEMENT c ANY>\r\n\r\n]>\r\n<!--* EMPTY element a has content *-->\r\n<root><a>should not have content here</a><b>\r\n   <c></c> \r\n   content of b element\r\n</b></root>\r\n\r\n",
  0, NULL
},
{
  "ibm39i02.xml",
  291,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (a,b)>\r\n  <!ELEMENT a EMPTY>\r\n  <!ELEMENT b (#PCDATA|c)* >\r\n  <!ELEMENT c ANY>\r\n\r\n]>\r\n<!--* root element have text content *-->\r\n<root>\r\n root can't have text content\r\n<a></a><b>\r\n   <c></c> \r\n   content of b element\r\n</b></root>\r\n\r\n",
  0, NULL
},
{
  "ibm39i03.xml",
  306,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (a,b)>\r\n  <!ELEMENT a EMPTY>\r\n  <!ELEMENT b (#PCDATA|c)* >\r\n  <!ELEMENT c ANY>\r\n]>\r\n<!--* illgal element in b's Mixed content *-->\r\n<root><a/><b>\r\n   <c></c> \r\n   content of b element\r\n   <a/>\r\n   could not have 'a' as 'b's content\r\n</b></root>\r\n\r\n",
  0, NULL
},
{
  "ibm39i04.xml",
  351,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (a,b)>\r\n  <!ELEMENT a EMPTY>\r\n  <!ELEMENT b (#PCDATA|c)* >\r\n  <!ELEMENT c ANY>\r\n  <!ELEMENT f EMPTY>\r\n]>\r\n<!--* element c has undeclared element as its ANY content *-->\r\n<root><a/><b>\r\n   <c><f/></c> \r\n   content of b element\r\n   <c>\r\n      <d>not declared in dtd</d>\r\n   </c>\r\n</b></root>\r\n\r\n",
  0, NULL
},
{
  "ibm41i01.xml",
  318,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA|b)* >\r\n  <!ELEMENT b (#PCDATA) >\r\n  <!ATTLIST b attr2 (abc|def) \"abc\">\r\n  <!ATTLIST b attr3 CDATA #FIXED \"fixed\">\r\n]>\r\n<root>\r\n  <b attr1=\"value1\" attr2=\"def\" attr3=\"fixed\">attr1 not declared</b>\r\n</root>\r\n<!--* testing VC:Attribute Value Type  *-->\r\n",
  0, NULL
},
{
  "ibm41i02.xml",
  365,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (PCDATA|b)* >\r\n  <!ELEMENT b (#PCDATA) >\r\n  <!ATTLIST b attr1 CDATA #REQUIRED>\r\n  <!ATTLIST b attr2 (abc|def) \"abc\">\r\n  <!ATTLIST b attr3 CDATA #FIXED \"fixed\">\r\n]>\r\n<root>\r\n  <b attr1=\"value1\" attr2=\"abc\" attr3=\"shoudbefixed\">attr3 value not fixed</b>\r\n</root>\r\n<!--* testing P41 VC: AtributeValueType*-->\r\n",
  0, NULL
},
{
  "ibm45i01.xml",
  549,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA|b)* >\r\n  <!ELEMENT b EMPTY>\r\n  <!ELEMENT not_unique ANY>\r\n  <!ELEMENT not_unique EMPTY>\r\n  <!ELEMENT not_unique (b,b) >\r\n  <!ELEMENT unique. ANY>\r\n  <!ATTLIST b attr1 CDATA #IMPLIED>\r\n  <!ATTLIST b attr2 CDATA #IMPLIED>\r\n  <!ATTLIST b attr3 CDATA #IMPLIED>\r\n]>\r\n<root>\r\n  <b/>without white space\r\n  <b /> with a white space\r\n  <b attr1=\"value1\" />\r\n  <b attr1=\"value1\" attr2=\"value2\" attr3 = \"value3\"/>\r\n</root>\r\n<!--* a invalid test: testing P45 VC unique element type decl  *-->\r\n",
  0, NULL
},
{
  "ibm49i01.xml",
  226,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm49i01.dtd\" [\r\n  <!ELEMENT root (a,b)>\r\n]>\r\n<root><a/><b>\r\n   <c></c >\r\n   content of b element\r\n</b></root>\r\n<!--* a invalid test: tests VC:Proper Group/PE Nesting in P49 *-->\r\n",
  0, NULL
},
{
  "ibm49i02.xml",
  226,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm49i02.dtd\" [\r\n  <!ELEMENT root (a,b)>\r\n]>\r\n<root><a/><b>\r\n   <c></c >\r\n   content of b element\r\n</b></root>\r\n<!--* a invalid test: tests VC:Proper Group/PE Nesting in P49 *-->\r\n",
  0, NULL
},
{
  "ibm50i01.xml",
  226,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm50i01.dtd\" [\r\n  <!ELEMENT root (a,b)>\r\n]>\r\n<root><a/><b>\r\n   <c></c >\r\n   content of b element\r\n</b></root>\r\n<!--* a invalid test: tests VC:Proper Group/PE Nesting in P50 *-->\r\n",
  0, NULL
},
{
  "ibm51i01.xml",
  225,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm51i01.dtd\" [\r\n  <!ELEMENT root ANY>\r\n]>\r\n<root>\r\n  <a> Element type a </a>\r\n  <b> Element type b </b>\r\n</root>\r\n<!--* a invalid test: tests P51 VC: Proper Group/PE Nesting *-->",
  0, NULL
},
{
  "ibm56i01.xml",
  370,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(negative Test)-->\r\n<!DOCTYPE tokenizer\r\n [\r\n <!ELEMENT tokenizer ANY>\r\n <!ATTLIST tokenizer UniqueName ID #REQUIRED>\r\n ]>\r\n<tokenizer UniqueName = \"@c999\">\r\nThis is a negative test for validity constraints\r\nthe value of the attribute with a type ID does not match the Name production\r\n</tokenizer>",
  0, NULL
},
{
  "ibm56i02.xml",
  459,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(negative Test)-->\r\n<!DOCTYPE tokenizer\r\n [\r\n <!ELEMENT tokenizer ANY>\r\n <!ELEMENT b EMPTY>\r\n <!ATTLIST b attr ID #REQUIRED>\r\n <!ATTLIST tokenizer UniqueName ID #REQUIRED>\r\n  ]>\r\n<tokenizer UniqueName = \"Ac999\">\r\n<b attr = \"Ac999\"></b>\r\nThis is a negative test for validity constraints\r\nthe value of the attribute with a type ID appears more than once in the XML document\r\n</tokenizer>\r\n",
  0, NULL
},
{
  "ibm56i03.xml",
  329,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity constraint check for Production 56(Negative Test)-->\r\n<!DOCTYPE tokenizer\r\n [\r\n <!ELEMENT tokenizer ANY>\r\n <!ATTLIST tokenizer UniqueName ID #FIXED \"AC1999\">\r\n ]>\r\n<tokenizer>\r\nThis is a Negative validity test for ID Attribute Default.\r\nGiving the attribute default as #FIXED\r\n</tokenizer>\r\n",
  0, NULL
},
{
  "ibm58i01.xml",
  560,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 58(Negative Test-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT blob (#PCDATA)>\r\n <!NOTATION base64 SYSTEM \"mimecode\">\r\n <!NOTATION uuencode SYSTEM \"uudecode\">\r\n <!NOTATION raw SYSTEM \"raw\">\r\n <!ATTLIST blob content-encoding NOTATION (base64|uuencode) #REQUIRED>\r\n ]>\r\n <test>\r\n<blob content-encoding = \"raw\"></blob>\r\nThe attribute values of type NOTATION does not match any of the notation names included in the\r\ndeclaration.All notation names in the declaration have been declared.\r\n</test>",
  0, NULL
},
{
  "ibm58i02.xml",
  547,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 58(Negative Test-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT blob (#PCDATA)>\r\n <!NOTATION base64 SYSTEM \"mimecode\">\r\n <!NOTATION uuencode SYSTEM \"uudecode\">\r\n <!ATTLIST blob content-encoding NOTATION (base64|uuencode|raw|ascii) #REQUIRED>\r\n ]>\r\n <test>\r\n<blob content-encoding = \"raw\"></blob>\r\nThe attribute values of type NOTATION does match any of the notation names included in the\r\ndeclaration, but some of notation names in the declaration have not been declared\r\n</test>",
  0, NULL
},
{
  "ibm59i01.xml",
  394,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 59-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT one EMPTY>\r\n <!ELEMENT two EMPTY>\r\n <!ELEMENT num EMPTY>\r\n <!ATTLIST num value (one|two) #IMPLIED>\r\n ]>\r\n <test>\r\n<num value = \"ONE\"></num>\r\nThis is a Negative test\r\nThe attribute values of type Enumeration does not match any of the Nmtoken tokens in the declaration.\r\n</test>",
  0, NULL
},
{
  "ibm60i01.xml",
  472,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 60-->\r\n<!DOCTYPE Java \r\n [\r\n <!ELEMENT Java ANY>\r\n <!ELEMENT one EMPTY>\r\n <!ELEMENT two EMPTY>\r\n <!ATTLIST one chapter CDATA #REQUIRED>\r\n <!ATTLIST two chapter CDATA #REQUIRED>\r\n ]>\r\n<Java>\r\n<one chapter=\"Introduction\"></one>\r\n<two></two>\r\nNegative test for Required Attribute. Some occurrence of an element with \r\nan attribute of #REQUIRED default declaration does not give the value of \r\nthose attribute\r\n</Java>",
  0, NULL
},
{
  "ibm60i02.xml",
  443,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 60-->\r\n<!DOCTYPE Java \r\n [\r\n <!ELEMENT Java ANY>\r\n <!ELEMENT one EMPTY>\r\n <!ATTLIST one chapter CDATA #FIXED \"Introduction\">\r\n  ]>\r\n<Java>\r\n<one chapter=\"JavaBeans\"></one>\r\nNegative Test\r\nAn attribute has a default value declared with the #FIXED keyword, \r\nand an instances of that attribute is given a value which is not \r\nthe same as the default value in the declaration. \r\n</Java>\r\n",
  0, NULL
},
{
  "ibm60i03.xml",
  363,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 60-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT a EMPTY>\r\n <!ELEMENT b EMPTY>\r\n <!ELEMENT attr EMPTY>\r\n <!ATTLIST attr value (a|b) \"c\"> \r\n  ]>\r\n<test>\r\nThe default value specified for an attribute does not meet the \r\nlexical constraints of the declared attribute type.\r\n</test>\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n",
  0, NULL
},
{
  "ibm60i04.xml",
  346,
  "<?xml  version=\"1.0\"?>\r\n<!-- validity test for Production 60-->\r\n<!DOCTYPE test\r\n [\r\n <!ELEMENT test ANY>\r\n <!ELEMENT a EMPTY>\r\n <!ELEMENT nametoken EMPTY>\r\n <!ATTLIST nametoken namevalue NMTOKEN \"@#$\"> \r\n  ]>\r\n<test>\r\nThe default value specified for an attribute does not meet the \r\nlexical constraints of the declared attribute type.\r\n</test>\r\n",
  0, NULL
},
{
  "ibm68i01.xml",
  211,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm68i01.dtd\" [\r\n  <!ELEMENT root (#PCDATA|a)* >\r\n]>\r\n<root>\r\n  pcdata content\r\n  <a attr1=\"xyz\"/>\r\n</root>\r\n<!--* a invalid test for P68 VC:Entity Declared *-->\r\n\r\n",
  0, NULL
},
{
  "ibm68i02.xml",
  211,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm68i02.dtd\" [\r\n  <!ELEMENT root (#PCDATA|a)* >\r\n]>\r\n<root>\r\n  pcdata content\r\n  <a attr1=\"xyz\"/>\r\n</root>\r\n<!--* a invalid test for P68 VC:Entity Declared *-->\r\n\r\n",
  0, NULL
},
{
  "ibm68i03.xml",
  231,
  "<?xml version=\"1.0\" standalone='no'?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA)* >\r\n  <!ENTITY % pe1 SYSTEM \"ibm68i03.ent\">\r\n  %pe1;\r\n]>\r\n<root>\r\n  pcdata content\r\n</root>\r\n<!--* a invalid test for P68 VC:Entity Declared *-->\r\n",
  0, NULL
},
{
  "ibm68i04.xml",
  231,
  "<?xml version=\"1.0\" standalone='no'?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA)* >\r\n  <!ENTITY % pe1 SYSTEM \"ibm68i04.ent\">\r\n  %pe1;\r\n]>\r\n<root>\r\n  pcdata content\r\n</root>\r\n<!--* a invalid test for P68 VC:Entity Declared *-->\r\n",
  0, NULL
},
{
  "ibm69i01.xml",
  211,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm69i01.dtd\" [\r\n  <!ELEMENT root (#PCDATA|a)* >\r\n]>\r\n<root>\r\n  pcdata content\r\n  <a attr1=\"xyz\"/>\r\n</root>\r\n<!--* a invalid test for P69 VC:Entity Declared *-->\r\n\r\n",
  0, NULL
},
{
  "ibm69i02.xml",
  211,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root SYSTEM \"ibm69i02.dtd\" [\r\n  <!ELEMENT root (#PCDATA|a)* >\r\n]>\r\n<root>\r\n  pcdata content\r\n  <a attr1=\"xyz\"/>\r\n</root>\r\n<!--* a invalid test for P69 VC:Entity Declared *-->\r\n\r\n",
  0, NULL
},
{
  "ibm69i03.xml",
  231,
  "<?xml version=\"1.0\" standalone='no'?>\r\n<!DOCTYPE root [\r\n  <!ELEMENT root (#PCDATA)* >\r\n  <!ENTITY % pe1 SYSTEM \"ibm69i03.ent\">\r\n  %pe1;\r\n]>\r\n<root>\r\n  pcdata content\r\n</root>\r\n<!--* a invalid test for P69 VC:Entity Declared *-->\r\n",
  0, NULL
},
{
  "ibm69i04.xml",
  232,
  "<?xml version=\"1.0\" standalone='no'?>\r\n<!DOCTYPE root  [\r\n  <!ELEMENT root (#PCDATA)* >\r\n  <!ENTITY % pe1 SYSTEM \"ibm69i04.ent\">\r\n  %pe1;\r\n]>\r\n<root>\r\n  pcdata content\r\n</root>\r\n<!--* a invalid test for P69 VC:Entity Declared *-->\r\n",
  0, NULL
},
{
  "ibm76i01.xml",
  387,
  "<?xml version=\"1.0\"?>\r\n<!DOCTYPE root \r\n[\r\n<!ELEMENT root (#PCDATA)>\r\n<!ATTLIST root att CDATA #IMPLIED>\r\n\r\n<!ENTITY % pe1 '<!ATTLIST root att2 CDATA \"&ge1;\">'>\r\n<!ENTITY ge1 \"attdefaultvalue\" >\r\n%pe1;\r\n\r\n<!--* notation JPGformat not declared *-->\r\n<!ENTITY ge2  SYSTEM \"image.jpg\" NDATA JPGformat>\r\n\r\n]>\r\n<root att2=\"any\" />\r\n<!-- a invalid test case: test P76 VC: Notation Declared -->",
  0, NULL
},
