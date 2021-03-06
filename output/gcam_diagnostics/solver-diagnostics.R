# LEGAL NOTICE
# This computer software was prepared by Battelle Memorial Institute,
# hereinafter the Contractor, under Contract No. DE-AC05-76RL0 1830
# with the Department of Energy (DOE). NEITHER THE GOVERNMENT NOR THE
# CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY
# LIABILITY FOR THE USE OF THIS SOFTWARE. This notice including this
# sentence must appear on any copies of this computer software.
# 
# EXPORT CONTROL
# User agrees that the Software will not be shipped, transferred or
# exported into any country or used in any manner prohibited by the
# United States Export Administration Act or any other applicable
# export laws, restrictions or regulations (collectively the "Export Laws").
# Export of the Software may require some form of license or other
# authority from the U.S. Government, and failure to obtain such
# export control license may result in criminal liability under
# U.S. laws. In addition, if the Software is identified as export controlled
# items under the Export Laws, User represents and warrants that User
# is not a citizen, or otherwise located within, an embargoed nation
# (including without limitation Iran, Syria, Sudan, Cuba, and North Korea)
#     and that User is not otherwise prohibited
# under the Export Laws from receiving the Software.
# 
# Copyright 2011 Battelle Memorial Institute.  All Rights Reserved.
# Distributed as open-source under the terms of the Educational Community 
# License version 2.0 (ECL 2.0). http://www.opensource.org/licenses/ecl2.php
# 
# For further details, see: http://www.globalchange.umd.edu/models/gcam/
#

# solver-diagnostics.R
#
# The solver diagnostics scripts use detailed data logged by the solver to explore
# the solver's progress. This script provides methods to help read, analyze, and
# visualize the key metrics to evaulate solution performance.
#
# See readme.md for additional documentation for using these tools.
#
# Robert Link


require(ggplot2)
require(data.table)
require(RColorBrewer)
require(graphics)
require(plyr)

### Create a collection of tables with the following structure
### list: one element for each GCAM model period.  for each period:
###      list:  one element for each variable type, for each variable
###           data frame: long-form table of values for each market by iteration number
###
### The columns in the lowest level table will be:
###      iteration, market id, solvable (T/F), value, name
###
###  Examples:
###  mydata <- read.trace.log(...)
###  mydata[[5]] (or mydata$`5`)  : all period 5 data
###  mydata[[5]]$price            : period 5 price data, with columns described above.
read.trace.log <- function(filename, key.filename=NA) {
    colnames <- c('period', 'iter', 'variable','mktid','solvable','value')
    colclasses <- c('integer', 'integer', 'factor','integer','logical','numeric')
    data <- fread(filename, sep=',', skip=3, header=FALSE, colClasses=colclasses)
    names(data) <- colnames
    setkey(data, period, mktid)

    if(is.na(key.filename))
        data$mktname <- as.factor('UNKNOWN')
    else {
        key.names <- c('period','mktid','mktname')
        key.classes <- c('integer','integer','factor')
        data.key <- fread(key.filename, sep=',', skip=3, header=FALSE, colClasses=key.classes)
        names(data.key) <- key.names
        setkey(data.key,period,mktid)
        data <- data[data.key]
    }

    ## split by period, and split each period by variable.  
    data <- data[period>0]             # drop bogus period 0
    data.by.period <- split(data, data$period)

    ## For each period, split it by variable and add iter and mktid as the primary and
    ## secondary sort keys (see data.table documentation for an explanaiton of how this
    ## works and why we need to do it).  Other functions here may rely on this keying, so
    ## don't change it.
    lapply(data.by.period,
           function(df) {
               lapply(split(df, df$variable), function(dt) {setkey(dt, iter, mktid)})
           }) 
}

fxcolormap <- function(n=51, controlpts=c(-10,-3,0,3,10)) {
    ### Create colormap for visualizing f(x) values.
    ###
    ### The hue will be piecewise over the four intervals defined by the
    ###  five control points (they need not be equally spaced).  The saturation
    ###  will ramp down from 1 to 0 on the interval from the first to the third 
    ###  control points, then back up to 1 on the rest of the interval.  If 
    ###  centered on 0 (like the default) this will result in a colormap where
    ###  the magnitude of f(x) is represented by the saturation and the hue localizes  
    ###  the value to one of four intervals.
    ###
    ### Inputs:
    ###   n - number of steps in the colormap
    ### controlpts - intervals over which the hue and saturation change.
    ###
    ### Return value:  Vector of colors
    xlo <- controlpts[1]
    x1  <- controlpts[2]
    xm  <- controlpts[3]
    x2  <- controlpts[4]
    xhi <- controlpts[5]
    
    x = seq(0,n-1) * (xhi-xlo)/(n-1) + xlo
    ## Hue is piecewise constant on four intervals
    h1 <- ifelse(x<x1, 0, 90/360)
    h2 <- ifelse(x>x2, 240/360, 180/360)
    H <- ifelse(x<xm, h1, h2)

    ## Use "option 2 for the saturation"
    eps <- 1.0e-8        # protect against roundoff giving vaues slightly larger than 1
    S <- ifelse(x<xm, 1-(x-xlo)/(xm-xlo), (x-xm)/(xhi-xm+eps))

    ## Constant 1 for value
    hsv(H, S, 1)
}

fxtransform <- function(ftol=1.0e-3) {
    ### Transform F(x) values for better visualization
    ###
    ### This transformation is a log scale for small values that switches to a linear 
    ###  scale for larger values.  Magnitudes less than
    ###  the solution tolerance are flushed to zero.  Magnitudes greater than
    ###  that but < 1 are divided by the tolerance, have the base-10 log taken, and
    ###  have their sign preserved.  Magnitudes > 1 are presented linearly (shifted
    ###  to be continuous with the small scale), and magnitudes >10 are clamped.
    function(x) {
        signx <- sign(x)
        magx  <- abs(x)
        xx    <- ifelse(magx < ftol, 0,
                        ifelse(magx < 1, log10(magx)-log10(ftol),
                               ifelse(magx < 10, (magx-1)-log10(ftol), 9-log10(ftol))))
        signx*xx                            # return value
    }
}

### Return a transform function that takes the magnitude and clips it to a maximum value
clipped.mag.transform <- function(maxmag=10) {
    function(x) {
        magx <- abs(x)
        ifelse(magx>maxmag, maxmag, magx)
    }
}

### Return a transform that clamps the values to two bounds
clamp.transform <- function(xmin=-100, xmax=100) {
    function(x) {
        pmax(xmin, pmin(x, xmax))
    }
}

### Return a transform function that gives sign(x)*log(x/xmin).
### x-values less than xmin are flushed to zero, and x-values greater
### than xmax are clipped to xmax
signed.log.transform <- function(xmin=1e-3, xmax=100) {
    function(x) {
        signx <- sign(x)
        absx  <- pmax( pmin(abs(x), xmax), xmin)
        signx * log10(absx/xmin)
    }
}

heatmap.gcam <- function(data, xform=identity, colors=c("white","blue"), title="", breaks=waiver(), solvable.only = FALSE) {
    ### Plot one of the traces in the solver-data-log as a heat map
    ###
    ### Inputs:  
    ###     data  - data frame containing the trace we want to plot (see read.trace.log())
    ###    xform  - transform to apply to the data (for easier interpretation).  Default is
    ###             identity (i.e., no transformation)
    ###    colors - colormap, a vector of color values
    ###    title  - string to print as the title of the plot
    ###    breaks - controls the breaks in the scale legend (see ggplot2::discrete_scale).
    ###             The default here usually does the right thing.

    if(solvable.only)
        data <- data[data$solvable,]
    
    nmkt <- max(data$mktid)
    if(nmkt < 10)
      intvl <- 1
    else if(nmkt < 500)
      intvl <- 10
    else if(nmkt < 2000)
      intvl <- 50
    else
      intvl <- 100
    mktbreaks <- seq(intvl,nmkt,intvl)
    niter <- max(data$iter)
    
    data$value <- xform(data$value)     # Apply the requested transformation to the data values.

    ggplot(data=data, aes(x=mktid, y=iter, fill=value)) + geom_raster() +
        scale_fill_gradientn(colours=colors, na.value="black", breaks=breaks) +
            ggtitle(title) +
            scale_x_continuous(breaks=mktbreaks) + 
            scale_y_continuous(breaks=seq(0,niter,20)) +
            xlab('Market ID')
}
  
calc.excess.dmnd <- function(data.period) {
  ## Add log-excess demand to data for a period.
  ## assumes keys have already been set
  supply <- data.period$supply
  demand <- data.period$demand
  
  newdata <- demand[supply, .(period, iter, variable, mktid, solvable, value, mktname, i.value)]
  newdata$value <- log10(newdata$value / (newdata$i.value+1.0e-8))
  newdata$variable <- 'log.excess.dmnd'
  newdata$i.value <- NULL
  newdata
}

### calculate a total derivative from deltax and deltafx
calc.total.deriv <- function(data.period) {
  ## assumes keys have already been set for variable tables  
  deltafx <- data.period$deltafx
  deltax <- data.period$deltax
  
  newdata <- deltax[deltafx, .(period, iter, variable, mktid, solvable, value, mktname, i.value)]
  newdata$value <- newdata$i.value / newdata$value
  newdata$i.value <- NULL
  newdata$variable <- "dfdx"
  newdata
}

####
#### The next few functions call heatmap.gcam with some tuned values that
#### (in my experience) produce readily interpretable plots.
####

### create a heat map of a single variable for a single period (i.e.,
### the bottom-level table in the list created by read.trace.log).
### This version is tuned for looking at fx.
heatmap.fx <- function(data, title="", breaks=NULL, ftol=1e-3) {
    fxform <- fxtransform(ftol)
    minval <- fxform(min(data$value, na.rm=TRUE))
    maxval <- fxform(max(data$value, na.rm=TRUE))

    ## set up control points as (min, x1, 0.0, x2, max), where x1 and
    ## x2 are the midpoints of [min,0] and [0,max], respectively
    cp <- rep(0.0,5)
    cp[1] <- minval   
    cp[2] <- 0.5*minval
    cp[4] <- 0.5*maxval
    cp[5] <- maxval
    
    ## use control point values as a default for legend labels, if
    ## none supplied.
    if(is.null(breaks))
        breaks <- cp

    heatmap.gcam(data, xform=fxform, colors=fxcolormap(n=51,controlpts=cp), 
                 title=title, breaks=breaks)
}

### heat map for deltax and deltafx
heatmap.delta <- function(data, title="", maxmag=5) {
    heatmap.gcam(data, xform=clipped.mag.transform(maxmag), title=title)
}

### heatmap for total derivative
heatmap.dfdx <- function(data, title="") {
    heatmap.gcam(data, xform=clamp.transform(-100,100),
                 colors=fxcolormap(101,c(-100, -20, 0, 20, 100)), title=title)
}

###
### Functions for looking at small numbers of markets
###
plotvars <- function(data, mktids, title="", skip=0, transforms=NULL, use.names=TRUE) {
  ## Plot all of the variables for a selection of markets in a single period
  ##   data : one period of data (i.e., the second level from read.trace.log)
  ##   mktids : vector of market ids
  ##   title: optional title for the plot
  ##   skip: Number of iterations to skip at the beginning.  Default is 0
  ## transforms : optinal list of transforms to apply to variables names(transforms)
  ##              indicates which variable the transforms apply to (for example,
  ##              transforms[['fx']] will get applied to the variable fx)
  ## use.names : determines whether the legend will display market names or market
  ##             id numbers.  Default is names.
  plotdata <- rbindlist(lapply(data, function(d) {d[mktid %in% mktids & iter>skip,]}))
  
  ## default transforms for this application, if none supplied by user
  if(is.null(transforms)) {
    cmt <- clipped.mag.transform(3)
    transforms <- list(fx=fxtransform(), deltafx=cmt, deltax=cmt)
  }
  
  ## apply transforms as necessary
  for(var in unique(plotdata$variable)) {
    if(var %in% names(transforms)) {
      ind <- plotdata$variable == var
      plotdata$value[ind] <- transforms[[var]](plotdata$value[ind])
    }
  }
    
  ## use market names or id numbers, as requested
  if(use.names)
    plotdata$cvar <- plotdata$mktname
  else
    plotdata$cvar <- as.factor(plotdata$mktid)
  
  ## select a color scheme.  Use Set1 from color brewer, unless there are
  ## too many markets being displayed, in which case use Set3 (using too
  ## many for set2 (>12) will cause colors to be repeated)
  nmkt <- length(mktids)
  if(nmkt<3) nmkt <- 3      # 3 is the minimum number supported.  
  if(nmkt <= 9)
    cpal <- 'Set1'
  else
    cpal <- 'Set3'
  
  ggplot(data=plotdata, aes(x=iter,y=value,color=cvar)) + geom_line() +
    facet_wrap(facets=~variable,scales='free_y') + 
    scale_color_brewer(name="market",palette=cpal)
}


###
### Functions for identifying markets to look at more closely
###

mkt.names <- function(perdata, mktids) {
    ## Look up market names by market id
    ##   perdata:  full dataset for a period
    ##   mktids:  vector of market ids to look up
    ##
    ##  return value: vector of market name strings
    ##
    ## Notes:  this function exploits the sorting imposed by the data.table
    ##    when the mktid field is the secondary sort key.  Changing the keying
    ##    on the log data may break this function.  (So don't do that!)
    perdata$price[mktids,mktname]       # Use price because it will have solvable & unsolvable markets
}

mkt.grep <- function(perdata, pattern, ...) {
    ## Find market ids for markets matching a regexp
    ##   perdata:  full dataset for a period
    ##   pattern:  character string containing a regular expression
    ##       ...:  Extra args are passed to grep.  Particularly useful
    ##             possiblilities include ignore.case, fixed, or perl
    ##
    ##  return value:  vector of market IDs whose names match the
    ##     regular expression supplied in pattern.

    ## limit to first iter so we don't have duplicates; use price
    ## because it has both solvable and unsolvable markets
    iter1 <- perdata$price[iter==1]

    mkts <- iter1$mktid[grep(pattern,iter1$mktname, ...)]
    names(mkts) <- iter1$mktname[mkts]
    mkts
}

final.mkt.extremes <- function(vardata, nmkt=5, final.iter=TRUE, findmax=TRUE) {
  ## Find the markets that have the largest (optionally smallest) values of a tracked variable.
  
  ## The main use for this function is looking at which markets are farthest from solution, 
  ## but it works with any of the variables we track.  In looking at which markets did or
  ## did not solve, it's often useful to look at the penultimate iteration, rather than the 
  ## final one, so we provide an option to do that.
  ##  vardata:  values for the period
  ##  nmkt: number of markets to return
  ##  final.iter:  if FALSE, use the penultimate iteration, instead of the final one
  ##  findmax:  If TRUE, find the largest values; otherwise, find the smallest
  ## Return value:  vector of market IDs
  
  iters <- vardata$iter
  niter <- max(iters)
  key.iter <- if(final.iter) niter else niter-1
  
  fxkey <- vardata[iters==key.iter,]
  
  ## sort by value
  srt <- order(abs(fxkey$value), decreasing=findmax)
  mkts.by.value <- fxkey$mktid[srt]
  names(mkts.by.value) <- fxkey$mktname[srt]
  mkts.by.value[1:nmkt]
}

overall.mkt.extremes <- function(vardata, nmkt=5, skip=0, findmax=TRUE) {
  ## Find markets with overall largest (optionally smallest) values of a tracked variable, as measured by the L2 metric
  ##  vardata:  values for the period
  ##  nmkt:  number of markets to return
  ##  skip:  number of iterations to skip at the beginning of the period
  ##  findmax:  If TRUE, find the largest l2 values; otherwise find the smallest
  ## Return value:  vector of market IDs
  vardata <- vardata[vardata$iter > skip,]
  fx.split <- split(vardata, vardata$mktid)
  l2 <- sapply(fx.split, function(d) {sum(d$value*d$value,na.rm=TRUE)})
  mktid <- sapply(fx.split, function(d){d$mktid[1]})
  mktname <- sapply(fx.split, function(d){d$mktname[1]})
  srt <- order(l2, decreasing=findmax)
  mkts <- as.integer(mktid[srt[1:nmkt]])
  names(mkts) <- mktname[srt[1:nmkt]]
  mkts
}
