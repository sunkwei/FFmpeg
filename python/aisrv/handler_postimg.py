#!/usr/bin/env python
#coding: utf-8


import os, sys, logging
from tornado import web
import cv2
import numpy as np



logger = logging.getLogger(__name__)



class handlerPostImg(web.RequestHandler):
    def post(self):
        ''' Content-Type: image/jpeg
        '''
        

        body = self.request.body
        jpeg = np.frombuffer(body, dtype=np.uint8)
        logger.debug("jpeg buf size: {}".format(jpeg.shape))
        img = cv2.imdecode(jpeg, 1)
        logger.debug("got img shape: {}".format(img.shape))
        
        # TODO: detection ...

        results = {"results": [1,2,3]}
        logger.info("... ")
        self.finish(results)

