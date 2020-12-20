gethr <- function(file="/scr/tmp/maclean/ttadjust_nov20_hr.txt")
{
    options(time.zone="UTC")
    x <- scan(file=file,what=list(y=1,m=1,d=1,hms="",
            toff= 1, tdiff=1, tdiffuncorr=1, tdiffmin=1, ndt=1))
    tx <- utime(paste(x$y,x$m,x$d,x$hms))

    xt <- dat(nts(matrix(c(x$tdiff, x$tdiffuncorr, x$tdiffmin,x$ndt),ncol=4,
            dimnames=list(NULL, c("tdiff", "tdiffuncorr", "tdiffmin", "I"))),
                tx,units=c(rep("sec",3),"")))
    xt
}

plot_ttadj <- function(xhr, npts=50, nper=10,
    title="WCR-TEST, PSFD", ...) 
{
    Sys.setenv(PROJECT="WCR-TEST")

    ifit <- 1
    for (i in seq(from=1, to=nrow(xhr), by=nper*npts)) {
        par(mfrow=c(3,1), mgp=c(2,0.5,0))
        xxhr <- xhr[i:(i+(nper*npts-1)),]
        # times in file are the raw data times. The adjusted times
        # are traw - tdiff
        dtraw <- diff(positions(xxhr))
        dtadj <- diff(positions(xxhr) - xxhr@data[,"tdiff"])

        nr <- nrow(xxhr)
        subt <- paste0(
            format(positions(xxhr[1,]),format="%Y %m %d %H:%M:%S",time.zone="UTC")," - ",
            format(positions(xxhr[nr,]),format="%H:%M:%S",time.zone="UTC"))

        par(mar=c(2,3,3,1))
        plot(0:(nr-1), xxhr[,"tdiffuncorr"], xlab="I", ylab="tdiff (sec)",
            main=paste(title, subt))
        par(mar=c(2,3,2,1))
        ylim <- range(c(dtraw,dtadj))
        plot(0:(nr-1), c(NA,dtraw), xlab="I", ylab="dtraw (sec)", ylim=ylim)
        par(mar=c(3,3,2,1))
        plot(0:(nr-1), c(NA,dtadj), xlab="I", ylab="dtadj (sec)", ylim=ylim)
        # legend(x=9*nr/10,y=ylim[1] + .5 * diff(ylim), c("tdiff","dtraw"),
        #     col=1:2, text.col=1:2, bty="n", pch=rep(1,2), yjust=0.5)

        par(mfrow=c(2,1), mgp=c(2,1,0), mar=c(3,3,3,1))
        hist(dtraw,xlim=ylim, main=paste0(title, ", dt raw, ", subt), xlab="sec")
        hist(dtadj,xlim=ylim, main=paste0(title, ", dt adj, ", subt), xlab="sec")
    }
}

