#!/usr/bin/env python
#coding: utf-8


import os, sys, logging, argparse
import os.path as osp
import sqlite3 as sq3
import json
from tornado import web, ioloop, httpserver



logger = logging.getLogger(__name__)



class handlerPostResult(web.RequestHandler):
    def post(self, token):
        ''' body:
            {
                "stamp": ...
                "....": ..
            }
        '''
        rs = json.loads(self.request.body)
        self.finish("ok")


    
class Application(web.Application):
    handlers = {
        (r"/post_result/(.+)", handlerPostResult),
    }
    super(Application, self).__init__(handlers)



def mainp(args):
    app = Application()
    server = httpserver.HTTPServer(app)
    server.bind(args.listen_port, args.bindip)
    logger.info("result server run at: %s:%d".format(args.bindip, args.listen_port))
    server.start()
    iorun = ioloop.IOLoop.instance()
    iorun.start()



if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    ap = argparse.ArgumentParser()
    ap.add_argument("--listen-port", type=int, default=9902)
    ap.add_argument("--bindip", type=str, default="0.0.0.0")
    args = ap.parse_args()
    mainp(args)
    