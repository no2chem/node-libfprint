/// <reference path="../typings/tsd.d.ts"/>

var binary = require('node-pre-gyp');
var path = require('path');
var PACKAGE_JSON = path.join(__dirname, '../package.json');
var binding_path = binary.find(path.resolve(PACKAGE_JSON));
var fprintbinding = require(binding_path);

var stream = require('stream');
var util = require('util');
var bunyan = require('bunyan');
var events = require('events');

var log;

export class fpreader {
    private wrapped;

    enroll_stages : number;
    supports_imaging : boolean;
    supports_identification: boolean;
    img_width: number;
    img_height: number;

    close = () => {
        this.wrapped.close();
    }

    constructor(fpinstance) {
        this.wrapped = fpinstance;

        //these values are static so we can grab them now
        this.enroll_stages = fpinstance.enroll_stages;
        this.supports_imaging = fpinstance.supports_imaging;
        this.supports_identification = fpinstance.supports_identification;
        this.img_width = fpinstance.img_width;
        this.img_height = fpinstance.img_height;
    }
}

export class fprint {

    // Initializes libfprint and returns 0 if successful.
    init() : number  {
        return fprintbinding.init();
    }

    discover() {
        var devices = [];
        fprintbinding.discover( function(handle, devid, drvtype, drvname, drvfullname)
                {
                    var thisdev = {
                        handle: handle,
                        deviceid: devid,
                        driver_type: drvtype,
                        driver: drvname,
                        driver_detail: drvfullname
                    };

                    devices.push(thisdev);
                });

        return devices;
    }

    get_reader(handle: number) {
        var reader = new fprintbinding.fpreader(handle);
        if (typeof reader != 'undefined')
        {
            return new fpreader(reader);
        }
        return null;
    }

    exit() : void {
        return fprintbinding.exit();
    }

    constructor () { }
}
