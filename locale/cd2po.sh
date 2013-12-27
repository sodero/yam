#!/bin/bash
#
# a shell script that converts an Amiga catalog description file (.cd)
# to an apropriate gettext (.po) file.
#
# Copyright 2013 Jens Maus <mail@jens-maus.de>
#


########################################################
# Script starts here
#

if [ "$1" != "" ]; then
  CDFILE="$1"
else
  echo "ERROR: missing cmdline argument"
  exit 1
fi

################################
# AWK scripts                  #
################################

# the following is an awk script that converts an
# Amiga-style catalog description file to a gettext
# PO-style translation template file.
read -d '' cd2pot << 'EOF'
BEGIN {
  tagfound=0
  multiline=0
  print "#"
  print "#, fuzzy"
  print "msgid \\"\\""
  print "msgstr \\"\\""
  print "\\"Project-Id-Version: YAM\\\\n\\""
  print "\\"Report-Msgid-Bugs-To: http://yam.ch/\\\\n\\""
  print "\\"POT-Creation-Date: 2012-12-23 10:29+0000\\\\n\\""
  print "\\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\\\n\\""
  print "\\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\\\n\\""
  print "\\"Language-Team: LANGUAGE <LL@li.org>\\\\n\\""
  print "\\"MIME-Version: 1.0\\\\n\\""
  print "\\"Content-Type: text/plain; charset=ISO-8859-1\\\\n\\""
  print "\\"Content-Transfer-Encoding: 8bit\\\\n\\""
  print "\\"Language: \\\\n\\""
}
{
  if($1 ~ /^MSG_.*\(.*\)/)
  {
    tagfound=1
    print "\\n#: " $0
    print "msgctxt \\"" $1 "\\""

    next
  }
  else if($1 ~ /^;/)
  {
    if(tagfound == 1)
    {
      print "msgstr \\"\\""
    }

    tagfound=0
    multiline=0
  }

  if(tagfound == 1)
  {
    // remove any backslash at the end of line
    gsub(/\\\\$/, "")

    # replace \e with \033
    gsub(/\\\\\\e/, "\\\\033")

    # replace plain " with \" but make
    # sure to check if \" is already there
    gsub(/\\\\"/, "\\"") # replace \" with "
    gsub(/"/, "\\\\\\"") # replace " with \"

    if(multiline == 0)
    {
      # the .po format doesn't allow empty msgid
      # strings, thus lets escape them with <EMPTY>
      if(length($0) == 0)
      {
        print "msgid \\"<EMPTY>\\""
      }
      else
      {
        print "msgid \\"" $0 "\\""
      }

      multiline=1
    }
    else
    {
      print "\\"" $0 "\\""
    }
  }
}
EOF

read -d '' ct2po << 'EOF'
BEGIN {
  tagfound=0
  multiline=0
  print "#"
  print "# Translators:"
  print "msgid \\"\\""
  print "msgstr \\"\\""
  print "\\"Project-Id-Version: yam\\\\n\\""
  print "\\"Report-Msgid-Bugs-To: http://yam.ch/\\\\n\\""
  print "\\"POT-Creation-Date: 2012-12-23 10:29+0000\\\\n\\""
  print "\\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\\\n\\""
  print "\\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\\\n\\""
  print "\\"Language-Team: LANGUAGE <LL@li.org>\\\\n\\""
  print "\\"MIME-Version: 1.0\\\\n\\""
  print "\\"Content-Type: text/plain; charset=UTF-8\\\\n\\""
  print "\\"Content-Transfer-Encoding: 8bit\\\\n\\""
  print "\\"Language: \\\\n\\""
}
{
  if($1 ~ /^MSG_.*$/)
  {
    tagfound=1
    multiline=0
    i=0

    # now we have to search in the CD file for the same string
    cmd="sed -e '1,/" $1 " /d' -e '/^;/,$d' YAM.cd"
    while((cmd |& getline output) > 0)
    {
      i++

      # remove any backslash at the end of line
      gsub(/\\\\$/, "", output)

      # replace \e with \033
      gsub(/\\\\\\e/, "\\\\033", output)

      # replace plain " with \" but make
      # sure to check if \" is already there
      gsub(/\\\\"/, "\\"", output) # replace \" with "
      gsub(/"/, "\\\\\\"", output) # replace " with \"

      if(length(output) == 0)
      {
        output="<EMPTY>"
      }

      if(i == 1)
      {
        print "\\n#: " $0
        print "msgctxt \\"" $1 "\\""
        print "msgid \\"" output "\\""
      }
      else
      {
        print "\\"" output "\\""
      }
    }
    close(cmd)

    if(i == 0)
    {
      tagfound=0
    }

    next
  }
  else if($1 ~ /^;/)
  {
    tagfound=0
    multiline=0
  }

  if(tagfound == 1)
  {
    # remove any backslash at the end of line
    gsub(/\\\\$/, "")

    # replace \e with \033
    gsub(/\\\\\\e/, "\\\\033")

    # replace plain " with \" but make
    # sure to check if \" is already there
    gsub(/\\\\"/, "\\"") # replace \" with "
    gsub(/"/, "\\\\\\"") # replace " with \"

    if(multiline == 0)
    {
      # the .po format doesn't allow empty msgid
      # strings, thus lets escape them with <EMPTY>
      if(length($0) == 0)
      {
        print "msgstr \\"<EMPTY>\\""
      }
      else
      {
        print "msgstr \\"" $0 "\\""
      }

      multiline=1
    }
    else
    {
      print "\\"" $0 "\\""
    }
  }
}
EOF

#awk "${cd2pot}" ${CDFILE}
iconv -c -f iso-8859-1 -t utf8 ${CDFILE} | awk "${ct2po}"
#iconv -c -f iso-8859-2 -t utf8 ${CDFILE} | awk "${ct2po}" # czech/polish/slovenian
#iconv -c -f windows-1251 -t utf8 ${CDFILE} | awk "${ct2po}" # russian
#iconv -c -f iso-8859-7 -t utf8 ${CDFILE} | awk "${ct2po}" # greek

exit 0