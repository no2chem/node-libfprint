// This script lists the available fingerprint readers on the system.

/// <reference path="../typings/tsd.d.ts"/>

var promise = require("bluebird");
var libfprint = promise.promisifyAll(require("../"));
var sprintf = require("sprintf-js").sprintf;

var verbose = false;
if (process.argv.length > 2)
{
    if (process.argv[2] == "-v")
    {
        verbose = true;
    }
    else
    {
        console.log("usage: lsfprint [-v]");
        process.exit(0);
    }
}

var fp = new libfprint.fprint();
console.log(sprintf("%8s %-8s %-8s %-32s", "handle", "type", "driver", "description"));

fp.init();
fp.discover().forEach(function (entry)
        {
            console.log(sprintf("%8d %-8s %-8s %-32s", entry.handle, entry.driver_type,  entry.driver, entry.driver_detail));

            if (verbose)
            {
                var reader = promise.promisifyAll(fp.get_reader(entry.handle));
                console.log(sprintf("\t Enroll stages: %d", reader.enroll_stages));
                console.log(sprintf("\t Supports imaging: %s", reader.supports_imaging));
                console.log(sprintf("\t Supports identification: %s", reader.supports_identification));
                console.log(sprintf("\t Image height: %d", reader.img_height));
                console.log(sprintf("\t Image width: %d", reader.img_width));
                reader.start_enrollAsync().then(
                    function (result)
                    {
                        console.log(result);
                    }
                    )
                    .catch(
                        function (err)
                        {
                            console.log("ERR");
                            console.log(err);
                        }
                    )

                    console.log("DONE");
            }
        });
console.log("BYE");
