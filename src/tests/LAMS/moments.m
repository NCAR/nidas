addpath utils;
addpath imat_top;
clear all;
sdev = 0.0;
spectra_file = '/home/lams/lams.bin';

disp('started')
while (1)

  fid = fopen(spectra_file, 'r');
  if (fid < 0)
    continue
  end

  % calculate file size
  fseek(fid, 0, 'eof');
  fsize = ftell(fid);
  fseek(fid, 0, 'bof');
  if (fsize ~= 4096)
    fclose(fid);
    continue
  end

  spectra = fread(fid, 512,'uint32');
  fclose(fid);
  sz = size(spectra);
  if (sz == 0)
    continue
  end

  title('Average Spectrum');
  plot(spectra);
  axis([0 512 0 500000000]);
  drawnow;
end
