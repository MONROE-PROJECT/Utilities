#!/usr/bin/env python

import sys
import hashlib
import simplejson as json
import logging
from docker import Client

if len(sys.argv)<2:
  print "usage: container-size.py [-v] container-url"
  print "       -v provides verbose output."

logger = logging.getLogger('container-size')
logger.addHandler(logging.StreamHandler(sys.stdout))

if "-v" in sys.argv:
  logger.setLevel(logging.DEBUG)

url = sys.argv[-1]

cli = Client(base_url='unix://var/run/docker.sock')

layers = {}
tags = {}
children = {}
parents = {}

images = cli.images(all=True)

for image in images:
  history = cli.history(image.get('Id'))

  for layer in history:
    digest = hashlib.md5(json.dumps(layer)).hexdigest()
    layer["digest"] = digest
    layers[digest] = layer
    tagged = layer.get('Tags')
    if tagged is not None: 
      for tag in tagged:
        tags[tag] = digest

  parent = None
  for layer in reversed(history):
    digest = layer.get('digest')
    children[parent] = children.get(parent, []) + [digest]
    parents[digest]=parent
    parent = digest

if not ":" in url:
  url = url + ":latest"

queried = tags.get(url)
if queried is None:
  logger.error("%s was not found in tags list." % url)
  logger.info(json.dumps(tags))
  sys.exit(1)

def browse_siblings(siblings, taglist):
  for sibling in siblings:
    tags = layers.get(sibling).get('Tags')
    if tags is not None:
      taglist.append(tags)
    kids = children.get(sibling)
    if kids is not None:
      taglist.extend(browse_siblings(kids, []))
  return taglist

sizes = 0
sibling_tags = []

while True:
  parent = parents.get(queried)
  if parent is None: 
    logger.info("No common ancestor found.")
    break
  siblings = children.get(parent)
  if len(siblings)>1:
    id = layers.get(parent).get('Id')
    logger.info("Common ancestor found (id %s)" % str(id))
    sibling_tags = browse_siblings (siblings,[])
    break
  else:
    id = layers.get(parent).get('Id')
    size = layers.get(parent).get('Size')
    sizes += size
    logger.info("added layer (id %s), size %i" % (id,size))
  queried = parent

print "%i bytes." % (sizes,)
logger.info("Siblings: %s" % (json.dumps(sibling_tags),))
logger.info("For more details, try 'docker run -it --rm -v /var/run/docker.sock:/var/run/docker.sock nate/dockviz images -i -n -t'")
