#!/usr/bin/env python
#coding: utf-8

import os, sys, logging, argparse
from tornado import web, httpserver, ioloop
import os.path as osp
from handler_postimg import handlerPostImg



logger = logging.getLogger(__name__)



class Application(web.Application):
    def __init__(self):
        handlers = [
            (r"/post_img", handlerPostImg),
        ]
        super(Application, self).__init__(handlers)


    
def mainp(args):
    app = Application()
    server = httpserver.HTTPServer(app)
    server.bind(args.listen_port, args.bindip)
    logger.info("ai server run: {}:{}".format(args.bindip, args.listen_port))
    server.start()
    iorun = ioloop.IOLoop.instance()
    iorun.start()



if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    ap = argparse.ArgumentParser()
    ap.add_argument("--listen-port", type=int, default=9901, help="the listen port, default 9901")
    ap.add_argument("--bindip", type=str, default="0.0.0.0", help="the bindip addr, default 0.0.0.0")
    args = ap.parse_args()
    mainp(args)
