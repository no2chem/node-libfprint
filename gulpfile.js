var gulp = require('gulp');
var ts = require('gulp-type');
var tsd = require('gulp-tsd');
var sourcemaps = require('gulp-sourcemaps');
var del = require('del');
var insert = require('gulp-insert');

var tsProject = ts.createProject({
    declarationFiles: true,
    noExternalResolve: true,
    sortOutput: true,
    module: "commonjs"
});

gulp.task('ts-compile', ['ts-typings'], function () {
    var tsResult = gulp.src(['src/*.ts', 'typings/**/*.ts'])
                       .pipe(sourcemaps.init())
                       .pipe(ts(tsProject));
    tsResult.dts.pipe(gulp.dest('./build/definitions'));
    return tsResult.js.pipe(sourcemaps.write())
                      .pipe(insert.prepend("#!/usr/bin/env node\n"))
                      .pipe(gulp.dest('./build'));
})

gulp.task('ts-typings', function (cb) {
    tsd({
        command: 'reinstall',
        config: './tsd.json'
    },cb);
});

gulp.task('clean', function(cb) {
    del(['build', 'typings'], cb);
});

gulp.task('default', function() {
    gulp.start('ts-compile', 'ts-typings');
});

gulp.task('watch', ['ts-compile'], function() {
    gulp.watch('src/*.ts', ['ts-compile']);
});
